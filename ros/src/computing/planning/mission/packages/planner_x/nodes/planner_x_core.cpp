/*
 *  Copyright (c) 2015, Nagoya University
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of Autoware nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "planner_x_core.h"
#include "RosHelpers.h"
#include "waypoint_follower/libwaypoint_follower.h"
#include "waypoint_follower/LaneArray.h"
#include <visualization_msgs/MarkerArray.h>
#include "geo_pos_conv.hh"
#include "PlannerHHandler.h"

namespace PlannerXNS
{

PlannerX_Interface* PlannerX_Interface::CreatePlannerInstance(const std::string& plannerName)
{
	if(plannerName.compare("PlannerH") == 0)
		return new PlannerH_Handler;
	else
		return 0;
}

PlannerX::PlannerX()
{
	clock_gettime(0, &m_Timer);
	m_counter = 0;
	m_frequency = 0;
	bInitPos = false;
	bGoalPos = false;
	bNewCurrentPos = false;
	bVehicleState = false;
	bNewDetectedObstacles = false;
	bTrafficLights = false;



	m_pPlanner = PlannerXNS::PlannerX_Interface::CreatePlannerInstance("PlannerH");

	tf::StampedTransform transform;
	RosHelpers::GetTransformFromTF("map", "world", transform);
	ROS_INFO("Origin : x=%f, y=%f, z=%f", transform.getOrigin().x(),transform.getOrigin().y(), transform.getOrigin().z());

	m_OriginPos.position.x  = transform.getOrigin().x();
	m_OriginPos.position.y  = transform.getOrigin().y();
	m_OriginPos.position.z  = transform.getOrigin().z();

	if(m_pPlanner)
		m_pPlanner->UpdateOriginTransformationPoint(m_OriginPos);

	m_PositionPublisher = nh.advertise<geometry_msgs::PoseStamped>("sim_pose", 10);
	m_PathPublisherRviz = nh.advertise<visualization_msgs::MarkerArray>("global_waypoints_mark", 10, true);
	m_PathPublisher = nh.advertise<waypoint_follower::LaneArray>("lane_waypoints_array", 10, true);

	// define subscribers.
	sub_current_pose 		= nh.subscribe("/current_pose", 				10,
			&PlannerX::callbackFromCurrentPose, 		this);
	sub_traffic_light 		= nh.subscribe("/light_color", 					10,
			&PlannerX::callbackFromLightColor, 			this);
	sub_obj_pose 			= nh.subscribe("/obj_car/obj_label", 			10,
			&PlannerX::callbackFromObjCar, 				this);
	point_sub 				= nh.subscribe("/vector_map_info/point_class", 	1,
			&PlannerX::callbackGetVMPoints, 			this);
	lane_sub 				= nh.subscribe("/vector_map_info/lane", 		1,
			&PlannerX::callbackGetVMLanes, 				this);
	node_sub 				= nh.subscribe("/vector_map_info/node", 		1,
			&PlannerX::callbackGetVMNodes, 				this);
	stopline_sub 			= nh.subscribe("/vector_map_info/stop_line", 	1,
			&PlannerX::callbackGetVMStopLines, 			this);
	dtlane_sub 				= nh.subscribe("/vector_map_info/dtlane", 		1,
			&PlannerX::callbackGetVMCenterLines, 		this);
	initialpose_subscriber 	= nh.subscribe("initialpose", 					10,
			&PlannerX::callbackSimuInitPose, 			this);
	goalpose_subscriber 	= nh.subscribe("move_base_simple/goal", 		10,
			&PlannerX::callbackSimuGoalPose, 			this);


}

PlannerX::~PlannerX()
{
	if(m_pPlanner)
		delete m_pPlanner;
}

void PlannerX::callbackSimuGoalPose(const geometry_msgs::PoseStamped &msg)
{
	PlannerHNS::WayPoint p;
	ROS_INFO("Target Pose Data: x=%f, y=%f, z=%f, freq=%d", msg.pose.position.x, msg.pose.position.y, msg.pose.position.z, m_frequency);

	m_pPlanner->UpdateGlobalGoalPosition(msg.pose);

	bGoalPos = true;
}

void PlannerX::callbackSimuInitPose(const geometry_msgs::PoseWithCovarianceStampedConstPtr &msg)
{
	PlannerHNS::WayPoint p;
	ROS_INFO("init Simulation Rviz Pose Data: x=%f, y=%f, z=%f, freq=%d", msg->pose.pose.position.x, msg->pose.pose.position.y, msg->pose.pose.position.z, m_frequency);

	m_InitPos.position  = msg->pose.pose.position;
	m_InitPos.orientation = msg->pose.pose.orientation;

	bInitPos = true;

//
//
//	std_msgs::Header h;
//	//h.stamp = current_time;
//	h.frame_id = "StartPosition";
//
//	geometry_msgs::PoseStamped ps;
//	ps.header = h;
//	ps.pose = msg->pose.pose;
//	m_PositionPublisher.publish(ps);
}

void PlannerX::callbackFromCurrentPose(const geometry_msgs::PoseStampedConstPtr& msg)
{
  // write procedure for current pose

	//PlannerHNS::WayPoint p;
	//ROS_INFO("Pose Data: x=%f, y=%f, z=%f, freq=%d", msg->pose.position.x, msg->pose.position.y, msg->pose.position.z, m_frequency);
	m_counter++;
	double dt = UtilityHNS::UtilityH::GetTimeDiffNow(m_Timer);
	if(dt >= 1.0)
	{
		m_frequency = m_counter;
		m_counter = 0;
		clock_gettime(0, &m_Timer);
	}

	geometry_msgs::Pose p = msg->pose;
	p.position.x = p.position.x - m_OriginPos.position.x;
	p.position.y = p.position.y - m_OriginPos.position.y;
	p.position.z = p.position.z - m_OriginPos.position.z;

	double distance = hypot(m_CurrentPos.position.y-p.position.y, m_CurrentPos.position.x-p.position.x);
	m_VehicleState.speed = distance/dt;
	if(m_VehicleState.speed>0.2 || m_VehicleState.shift == AW_SHIFT_POS_DD )
		m_VehicleState.shift = AW_SHIFT_POS_DD;
	else if(m_VehicleState.speed<-0.2)
		m_VehicleState.shift = AW_SHIFT_POS_RR;
	else
		m_VehicleState.shift = AW_SHIFT_POS_NN;

	m_CurrentPos.position  = p.position;
	m_CurrentPos.orientation = p.orientation;

	bNewCurrentPos = true;

}

void PlannerX::callbackFromLightColor(const runtime_manager::traffic_light& msg)
{
  // write procedure for traffic light
}

void PlannerX::callbackFromObjCar(const cv_tracker::obj_label& msg)
{
  // write procedure for car obstacle
}

void PlannerX::callbackGetVMPoints(const map_file::PointClassArray& msg)
{
	ROS_INFO("Received Map Points");
	m_AwMap.points = msg;
	m_AwMap.bPoints = true;
}

void PlannerX::callbackGetVMLanes(const map_file::LaneArray& msg)
{
	ROS_INFO("Received Map Lane Array");
	m_AwMap.lanes = msg.lanes;
	m_AwMap.bLanes = true;
}

void PlannerX::callbackGetVMNodes(const map_file::NodeArray& msg)
{
	ROS_INFO("Received Map Nodes");


}

void PlannerX::callbackGetVMStopLines(const map_file::StopLineArray& msg)
{
	ROS_INFO("Received Map Stop Lines");
}

void PlannerX::callbackGetVMCenterLines(const map_file::DTLaneArray& msg)
{
	ROS_INFO("Received Map Center Lines");
	m_AwMap.dtlanes = msg.dtlanes;
	m_AwMap.bDtLanes = true;
}

void PlannerX::PlannerMainLoop()
{
	if(!m_pPlanner)
	{
		ROS_ERROR("Can't Create Planner Object ! ");
		return;
	}

	ros::Rate loop_rate(1);

	while (ros::ok())
	{
		ros::spinOnce();

		if(m_AwMap.bDtLanes && m_AwMap.bLanes && m_AwMap.bPoints)
			m_pPlanner->UpdateRoadMap(m_AwMap);

		AutowareBehaviorState behState;
		visualization_msgs::MarkerArray marker_array;
		waypoint_follower::LaneArray lane_array;
		bool bNewPlan = false;

		if(bInitPos && bGoalPos)
		{
			if(m_VehicleState.shift == AW_SHIFT_POS_DD)
				bNewPlan = m_pPlanner->GeneratePlan(m_CurrentPos,m_DetectedObstacles, m_TrafficLights, m_VehicleState,
						behState, marker_array, lane_array);
			else
				bNewPlan = m_pPlanner->GeneratePlan(m_InitPos,m_DetectedObstacles, m_TrafficLights, m_VehicleState,
						behState, marker_array, lane_array);
		}

		if(bNewPlan)
		{
			if(lane_array.lanes.size()>0)
				std::cout << "New Plan , Path size = " << lane_array.lanes.at(0).waypoints.size() << std::endl;
			m_PathPublisher.publish(lane_array);
			m_PathPublisherRviz.publish(marker_array);
		}

		ROS_INFO("Main Loop Step");
		loop_rate.sleep();
	}
}

}
