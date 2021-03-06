// ESE 680
// RRT assignment
// Author: Hongrui Zheng

// This file contains the class definition of tree nodes and RRT
// Before you start, please read: https://arxiv.org/pdf/1105.1186.pdf
// Make sure you have read through the header file as well
#include "rrt/rrt.h"
//#include "rrt/pose_2d.hpp"

#include <random>

#include <iostream>
using namespace std;

// Destructor of the RRT class
RRT::~RRT()
{
    // Do something in here, free up used memory, print message, etc.
    ROS_INFO("RRT shutting down");
}

// Constructor of the RRT class
RRT::RRT(ros::NodeHandle &nh) : nh_(nh), gen((std::random_device())())
{
    nh_.getParam("rrt/pose_topic", pose_topic);
    nh_.getParam("rrt/scan_topic", scan_topic);
    nh_.getParam("rrt/path_topic", path_topic);
    nh_.getParam("rrt/map_buffed_topic", map_topic);
    nh_.getParam("rrt/clicked_point_topic", clicked_point_topic);
    nh_.getParam("rrt/nav_goal_topic", nav_goal_topic);
    nh_.getParam("rrt/marker_topic", marker_topic);
    nh_.getParam("rrt/tree_topic", tree_topic);

    nh_.getParam("rrt/rrt_steps", rrt_steps);
    nh_.getParam("rrt/collision_accuracy", collision_accuracy);
    nh_.getParam("rrt/step_length", step_length);
    nh_.getParam("rrt/goal_threshold", goal_threshold);
    nh_.getParam("rrt/dRRT", dRRT);
    nh_.getParam("rrt/rrt_bias", rrt_bias);

    ROS_INFO_STREAM("rrt_steps: " << rrt_steps);


    bool real = false;
    std::string real_pose_topic = "";
    nh_.getParam("/real", real);
    nh_.getParam("/real_pose_topic", real_pose_topic);

    if(real){
        pose_topic = real_pose_topic;
        ROS_INFO_STREAM("Real RRT launch");
    }


    ROS_INFO_STREAM("rrt_steps: " << rrt_steps << " pose_topic: " << pose_topic);
    // ROS publishers
    vis_pub = nh_.advertise<visualization_msgs::Marker>(marker_topic, 0);
    tree_pub_ = nh_.advertise<std_msgs::Float64MultiArray>(tree_topic, 0);
    path_pub_ = nh_.advertise<std_msgs::Float64MultiArray>(path_topic, 10);

    // ROS subscribers
    pf_sub_ = nh_.subscribe(pose_topic, 10, &RRT::pf_callback, this);
    scan_sub_ = nh_.subscribe(scan_topic, 10, &RRT::scan_callback, this);
    map_sub = nh_.subscribe(map_topic, 1, &RRT::map_callback, this);
    click_sub = nh_.subscribe(clicked_point_topic, 1, &RRT::clicked_point_callback, this);
    nav_goal_sub = nh_.subscribe(nav_goal_topic, 1, &RRT::nav_goal_callback, this);

    gen = std::mt19937(123);
    x_dist = std::uniform_real_distribution<double>(-35.0, 35.0);
    y_dist = std::uniform_real_distribution<double>(-35.0, 35.0);

    rrt_tree_build = false;
    //starting goal
    q_goal.push_back(22.0);
    q_goal.push_back(-1.8);

    ROS_INFO("Created new RRT Object.");
}

// The loop where the rrt tree is created
    // Args:
    //    
    // Returns:
void RRT::rrt_loop()
{
    tree.clear();
    Node start;
    start.x = pose_x;
    start.y = pose_y;
    start.cost = 0;
    start.parent = -1;
    start.old_parent = -1;
    start.is_root = true;
    tree.push_back(start);

    int counter = 0;
    int counterS = 0;

    while (counter < rrt_steps)
    {
        
        std::vector<double> sampled_point = sample();
        int near = nearest(tree, sampled_point);
        Node x_new = steer(tree[near], sampled_point);
        if (sampled_point[0]==q_goal[0] && sampled_point[1]==q_goal[1] ){
            counterS++;
        }
        if (!check_collision(tree[near], x_new))
        {  
            if(true)                           //true --> RRT* | false --> RRT
            {
                rrt_star(tree,x_new,near);
            }
            else
            {
                tree.push_back(x_new);
            }            
        }
        if (is_goal(x_new))
        {
            path.clear();          
            path = find_path(tree, x_new);
        }        
        counter++;
//DEBUG-------------------------------------------------------------------
        if (counter*100/rrt_steps != (counter+1)*100/rrt_steps){
             ROS_INFO_STREAM("RRT Process :"<< counter*100/rrt_steps<<"%\n"
                             "Percentage that sampled Point == q_goal: "<<counterS*100/counter<<"%");
        }
    }
    if (!path.empty())
    {
        Node goalNode = path.front();
        path.clear();          
        path = find_path(tree, goalNode);
        ROS_INFO_STREAM("RRT has finished.\nA path has been found!");
    }else
    {
        ROS_INFO_STREAM("RRT has finished.\nNo path has been found!");
    }
    
    rrt_tree_build = true;
}

// The scan callback, update your occupancy grid here
    // Args:
    //    scan_msg (*LaserScan): pointer to the incoming scan message
    // Returns:
void RRT::scan_callback(const sensor_msgs::LaserScan::ConstPtr &scan_msg)
{
    /*std_msgs::Float64MultiArray path_msg;

    for (int i = 0; i < path.size(); i++)
    {

        path_msg.data.push_back(path[i].x);
        path_msg.data.push_back(path[i].y);
    }
    path_pub_.publish(path_msg);
    pub_tree(tree);*/
}

//not being called currently

// The pose callback when subscribed to particle filter's inferred pose
    // The RRT main loop happens here
    // Args:
    //    pose_msg (*PoseStamped): pointer to the incoming pose message
    // Returns:
void RRT::pf_callback(const geometry_msgs::PoseStamped::ConstPtr &pose_msg)
{
    
    //
    //double distance_to_nearest = distance_transform(pose_msg->pose.position.x, pose_msg->pose.position.y);
    //ROS_INFO_STREAM("Distance: " << distance_to_nearest);
    //double distance_wall = trace_ray(pose_msg->pose.position.x, pose_msg->pose.position.y,0);
    //ROS_INFO_STREAM("Distance: " << distance_wall);

    // tree as std::vector
    std::vector<Node> tree;
    pose_x = pose_msg->pose.position.x;
    pose_y = pose_msg->pose.position.y;


    //Kopiert aus scan callback. Vielleicht falsch hier
    std_msgs::Float64MultiArray path_msg;

    for (int i = 0; i < path.size(); i++)
    {

        path_msg.data.push_back(path[i].x);
        path_msg.data.push_back(path[i].y);
    }
    path_pub_.publish(path_msg);
    pub_tree(tree);

    // TODO: fill in the RRT main loop

    // path found as Path message
}

void RRT::clicked_point_callback(const geometry_msgs::PointStamped &pose_msg)
{

    double distance_to_nearest = distance_transform(pose_msg.point.x, pose_msg.point.y);
    ROS_INFO_STREAM("Distance: " << distance_to_nearest);
    //double distance_wall = trace_ray(pose_msg->pose.position.x, pose_msg->pose.position.y,0);
    //ROS_INFO_STREAM("Distance: " << distance_wall);
}

void RRT::nav_goal_callback(const geometry_msgs::PoseStamped &pose_msg)
{
    double x = pose_msg.pose.position.x;
    double y = pose_msg.pose.position.y;
    q_goal.clear();
    q_goal.push_back(x);
    q_goal.push_back(y);
    ROS_INFO_STREAM("New nav goal set to X: " << x << " Y: " << y);
    tree.clear();
    path.clear();
    rrt_loop();
}

// This method returns a sampled point from the free space
    // You should restrict so that it only samples a small region
    // of interest around the car's current position
    // Args:
    // Returns:
    //     sampled_point (std::vector<double>): the sampled point in free space
std::vector<double> RRT::sample()
{
    std::vector<double> sampled_point;   
    sampled_point.push_back(x_dist(gen));
    sampled_point.push_back(y_dist(gen));
    

    if (rand()<=RAND_MAX*rrt_bias && path.empty()){
        sampled_point[0] = q_goal[0];
        sampled_point[1] = q_goal[1];
    }

    return sampled_point;
}

// This method returns the nearest node on the tree to the sampled point
    // Args:
    //     tree (std::vector<Node>): the current RRT tree
    //     sampled_point (std::vector<double>): the sampled point in free space
    // Returns:
    //     nearest_node (int): Index of nearest node on the tree
int RRT::nearest(std::vector<Node> &tree, std::vector<double> &sampled_point)
{
    
    int nearest_node = 0;
    double nearest_distance = 9999999;
    for (int i = 0; i < tree.size(); i++)
    {

        double distance = distanceNodePoint(tree[i], sampled_point);
        if (distance < nearest_distance)
        {
            nearest_node = i;
            nearest_distance = distance;
        }
    }
    return nearest_node;
}

// This method returns the distance between a node and a point in space
    // Args:
    //     node (Node): the node from where to measure
    //     point (std::vector<double>): the point in space
    // Returns:
    //     distance (double): the distance between node and point
double RRT::distanceNodePoint(Node node, std::vector<double> &point)
{
    double xdif = point[0] - node.x;
    double ydif = point[1] - node.y;

    return sqrt(xdif * xdif + ydif * ydif);
}

/*  The function steer:(x,y)->z returns a point such that z is “closer”
    to y than x is. The point z returned by the function steer will be
    such that z minimizes ||z−y|| while at the same time maintaining
    ||z−x|| <= max_expansion_dist, for a prespecified max_expansion_dist > 0

    basically, expand the tree towards the sample point (within a max dist)

    Args:
       nearest_node (Node): nearest node on the tree to the sampled point
       sampled_point (std::vector<double>): the sampled point in free space
    Returns:
       new_node (Node): new node created from steering
*/
Node RRT::steer(Node &nearest_node, std::vector<double> &sampled_point)
{
    Node new_node;
    new_node.is_root = false;

    double distance = distanceNodePoint(nearest_node, sampled_point);
    if (distance <= step_length)
    {

        new_node.x = sampled_point[0];
        new_node.y = sampled_point[1];
    }
    else
    {
        new_node.x = nearest_node.x + (sampled_point[0] - nearest_node.x) * step_length / distance;
        new_node.y = nearest_node.y + (sampled_point[1] - nearest_node.y) * step_length / distance;
        //ROS_INFO_STREAM("step");
    }

    return new_node;
}

bool RRT::check_collision(Node &nearest_node, Node &new_node)
{
    // This method returns a boolean indicating if the path between the
    // nearest node and the new node created from steering is collision free
    // Args:
    //    nearest_node (Node): nearest node on the tree to the sampled point
    //    new_node (Node): new node created from steering
    // Returns:
    //    collision (bool): true if in collision, false otherwise

    for (double i = collision_accuracy; i <= 1; i += collision_accuracy)
    {

        double x = nearest_node.x + (new_node.x - nearest_node.x) * i;
        double y = nearest_node.y + (new_node.y - nearest_node.y) * i;
        if (distance_transform(x, y) == 0)
        {
            return true;
        }
    }

    return false;
}

// bool RRT::check_collision(Node &nearest_node, Node &new_node)
// {
//     // This method returns a boolean indicating if the path between the
//     // nearest node and the new node created from steering is collision free
//     // Args:
//     //    nearest_node (Node): nearest node on the tree to the sampled point
//     //    new_node (Node): new node created from steering
//     // Returns:
//     //    collision (bool): true if in collision, false otherwise

//     for (double i = collision_accuracy; i <= 1; i += collision_accuracy)
//     {

//         double x = nearest_node.x + (new_node.x - nearest_node.x) * i;
//         double y = nearest_node.y + (new_node.y - nearest_node.y) * i;
//         if (distance_transform(x, y) == 0)
//         {
//             return true;
//         }
//     }

//     return false;
// }

bool RRT::is_goal(Node &latest_added_node)
{
    // This method checks if the latest node added to the tree is close
    // enough (defined by goal_threshold) to the goal so we can terminate
    // the search and find a path
    // Args:
    //   latest_added_node (Node): latest addition to the tree
    //   goal_x (double): x coordinate of the current goal
    //   goal_y (double): y coordinate of the current goal
    // Returns:
    //   close_enough (bool): true if node close enough to the goal
    bool close_enough = false;
    
    double dist_goal = distanceNodePoint(latest_added_node, q_goal);
    if (dist_goal < goal_threshold)
    {
        close_enough = true;
    }

    return close_enough;
}

std::vector<Node> RRT::find_path(std::vector<Node> &tree, Node &latest_added_node)
{
    //ROS_INFO_STREAM("search path");
    // This method traverses the tree from the node that has been determined
    // as goal
    // Args:
    //   latest_added_node (Node): latest addition to the tree that has been
    //      determined to be close enough to the goal
    // Returns:
    //   path (std::vector<Node>): the vector that represents the order of
    //      of the nodes traversed as the found path
    std::vector<Node> found_path;
    Node n = latest_added_node;
    int count = 0;
    while (!n.is_root)
    {
        if (count < rrt_steps){
            found_path.push_back(n);
            n = tree[n.parent];
            count++;
        }else
        {
            ROS_ERROR_STREAM("path counter exceeds the rrt_steps "<<rrt_steps);
            break;
        }
        
    }
    found_path.push_back(n);

    return found_path;
}

// RRT* methods--------------------------------------------------------

// This method looks if there is a node in the new_nodes neighbourhood, 
    // that has a lower cost than near, if yes the node is
    // the new parent of new_node, else near becomes new_nodes parent.
    // The nodes in the neighbourhood are also checked if they would have
    // a lower cost, when new_node where its parent and switches if so.
    // Args:
    //   tree (std::vector<Node>): the complete tree of nodes
    //   x_new (Node): the new node to be added in the tree
    //   near (int): the index of the nearest node to the samplepoint
    // Returns:
void RRT::rrt_star(std::vector<Node> &tree, Node &x_new, int &near){
    std::vector<int> neighbourhood = RRT::near(tree, x_new); 

    int min_node = near;
    x_new.old_parent = near;
    double cost_min = cost(tree[near], x_new);
    for (int i:neighbourhood)   //check if there is a parent with lower cost in the neighbourhood
    {                
        double cost_neighbour = cost(tree[i],x_new);          
        if(!check_collision(tree[i], x_new) && cost_neighbour<cost_min)
        {
            min_node = i;
            cost_min = cost_neighbour;
        }
    }
    x_new.cost = cost_min;                   
    x_new.parent = min_node;
    tree.push_back(x_new);
    for (int i:neighbourhood)   //rewire the tree if there is a better path
    {    
        Node *neighbour = &tree[i];
        double cost_newroute = cost(x_new,*neighbour); 
        if(!check_collision(x_new,*neighbour) && cost_newroute<(*neighbour).cost){
            if(x_new.parent==i)
            {
                ROS_ERROR_STREAM("Child and parent are the same!");
            }                     
            (*neighbour).parent = tree.size()-1;   //make x_new the new parent of neighbour.
        }
    }
}

// This method returns the cost associated with a node
    // Args:
    //    parent (Node): the parent of node
    //    node (Node): the node the cost is calculated for
    // Returns:
    //    cost (double): the cost value associated with the node
double RRT::cost(Node &parent, Node &node)
{
        
    double cost = parent.cost + line_cost(parent,node);
// DEBUG---------------------------------------------------------------------------------------
    /*if (cost==1.6){
    ROS_INFO_STREAM("line cost: "<<line_cost(parent,node));
    ROS_INFO_STREAM("parent cost: "<<parent.cost);
    ROS_INFO_STREAM("cost: "<<cost);
    }*/
    return cost;
}

// This method returns the cost of the straight line path between two nodes
    // Args:
    //    n1 (Node): the Node at one end of the path
    //    n2 (Node): the Node at the other end of the path
    // Returns:
    //    cost (double): the cost value associated with the path
double RRT::line_cost(Node &n1, Node &n2)
{
    

    double cost = sqrt(pow(n1.x-n2.x,2)+pow(n1.y-n2.y,2));

// DEBUG---------------------------------------------------------------------------------------
    //ROS_INFO_STREAM("Node1 x: "<<n1.x<<"Node1 y: "<<n1.y<<"Node2 x: "<<n2.x<<"Node2 y: "<<n2.y<<"line cost: "<<cost);
    return cost;
}

// This method returns the set of Nodes in the neighborhood of a node.
// currently it has a time complexity of O(n^2), but 
// Args:
//   tree (std::vector<Node>): the current tree
//   node (Node): the node to find the neighborhood for
// Returns:
//   neighborhood (std::vector<int>): the IDs of the nodes in the neighborhood
std::vector<int> RRT::near(std::vector<Node> &tree, Node &node)
{
    

    // radius of the neighbourhood
    //double radius = dRRT*tree.size();
    int n = tree.size();
    double radius = min(dRRT*(log(n)/n)*1/2,step_length)+0.5; //to enlarge the neighbourhood a bit, +0.5.
//DEBUG---------------------------------------------------------------------------------------------
    //ROS_INFO_STREAM("radius: "<<radius);
    
    double dist;
    std::vector<int> neighborhood;

    for(int i= 0; i<tree.size(); i++){
        dist = sqrt(fabs(pow(node.x-tree[i].x,2)+pow(node.y-tree[i].y,2)));
        if(dist < radius){
            neighborhood.push_back(i);
        }
    }

    return neighborhood;
}
// For visualization
void RRT::pub_tree(std::vector<Node> &tree)
{
    // publish the current tree as a float array topic
    // published as [n1.x, n1.y, n1.parent.x, n1.parent.y, ......]
    int tree_length = tree.size();
    std_msgs::Float64MultiArray tree_msg;
    for (int i = 1; i < tree_length; i++)
    {
        double x = tree[i].x, y = tree[i].y;
        double px, py,opx,opy;
        if (tree[i].parent == -1)
        {
            px = 0.0;
            py = 0.0;
        }
        else
        {
            if(abs(tree[i].parent)<tree.size()){
                px = tree[tree[i].parent].x, py = tree[tree[i].parent].y;
            }else
            {
                //ROS_ERROR_STREAM("The parent of node" <<i<<" is not in the tree. It's assigned parent is: "<<tree[i].parent);
                break;
            }
            
        }
        if (tree[i].old_parent == -1)
        {
            opx = 0.0;
            opy = 0.0;
        }
        else
        {
            if(abs(tree[i].old_parent)<tree.size()){
                opx = tree[tree[i].old_parent].x, opy = tree[tree[i].old_parent].y;
            }else
            {
                //ROS_ERROR_STREAM("The parent of node" <<i<<" is not in the tree. It's assigned parent is: "<<tree[i].parent);
                break;
            }
            
        }
        tree_msg.data.push_back(x);
        tree_msg.data.push_back(y);
        //parent
        tree_msg.data.push_back(px);
        tree_msg.data.push_back(py);
        //old parent
        tree_msg.data.push_back(opx);
        tree_msg.data.push_back(opy);
    }
    tree_pub_.publish(tree_msg);
}

void RRT::map_callback(const nav_msgs::OccupancyGrid &msg)
{
    // Fetch the map parameters
    height = msg.info.height;
    width = msg.info.width;
    resolution = msg.info.resolution;
    // Convert the ROS origin to a pose

    origin.x = msg.info.origin.position.x;
    origin.y = msg.info.origin.position.y;
    geometry_msgs::Quaternion q = msg.info.origin.orientation;
    tf2::Quaternion quat(q.x, q.y, q.z, q.w);
    origin.theta = tf2::impl::getYaw(quat);

    // Convert the map to probability values
    std::vector<double> map(msg.data.size());
    for (size_t i = 0; i < height * width; i++)
    {
        if (msg.data[i] > 100 or msg.data[i] < 0)
        {
            map[i] = 0.5; // Unknown
        }
        else
        {
            map[i] = msg.data[i] / 100.;
        }
    }
    // Assign parameters

    origin_c = std::cos(origin.theta);
    origin_s = std::sin(origin.theta);
    double free_threshold = 0.8;
    // Threshold the map
    dt = std::vector<double>(map.size());
    for (size_t i = 0; i < map.size(); i++)
    {
        if (0 <= map[i] and map[i] <= free_threshold)
        {
            dt[i] = 99999; // Free
        }
        else
        {
            dt[i] = 0; // Occupied
        }
    }
    //DistanceTransform::distance_2d(dt, width, height, resolution);

    /*// Send the map to the scanner
            scan_simulator.set_map(
                map,
                height,
                width,
                resolution,
                origin,
                map_free_threshold);
            map_exists = true;*/

    ROS_INFO_STREAM("Map initialized");
    if (!rrt_tree_build)
    {
        rrt_loop();
    }
}

double RRT::trace_ray(double x, double y, double theta_index) const
{
    // Add 0.5 to make this operation round rather than floor
    int theta_index_ = theta_index + 0.5;
    //double s = sines[theta_index_];
    //double c = cosines[theta_index_];
    double s = std::sin(theta_index_);
    double c = std::cos(theta_index_);

    // Initialize the distance to the nearest obstacle
    double distance_to_nearest = distance_transform(x, y);
    double total_distance = distance_to_nearest;
    double ray_tracing_epsilon = 0.0001;
    while (distance_to_nearest > ray_tracing_epsilon)
    {
        // Move in the direction of the ray
        // by distance_to_nearest
        x += distance_to_nearest * c;
        y += distance_to_nearest * s;

        // Compute the nearest distance at that point
        distance_to_nearest = distance_transform(x, y);
        total_distance += distance_to_nearest;
    }

    return total_distance;
}

double RRT::distance_transform(double x, double y) const
{
    // Convert the pose to a grid cell
    int cell = xy_to_cell(x, y);
    if (cell < 0)
        return 0;

    return dt[cell];
}

void RRT::xy_to_row_col(double x, double y, int *row, int *col) const
{
    // Translate the state by the origin
    double x_trans = x - origin.x;
    double y_trans = y - origin.y;

    // Rotate the state into the map
    double x_rot = x_trans * origin_c + y_trans * origin_s;
    double y_rot = -x_trans * origin_s + y_trans * origin_c;

    // Clip the state to be a cell
    if (x_rot < 0 or x_rot >= width * resolution or
        y_rot < 0 or y_rot >= height * resolution)
    {
        *col = -1;
        *row = -1;
    }
    else
    {
        // Discretize the state into row and column
        *col = std::floor(x_rot / resolution);
        *row = std::floor(y_rot / resolution);
    }
}

int RRT::row_col_to_cell(int row, int col) const
{
    return row * width + col;
}

int RRT::xy_to_cell(double x, double y) const
{
    int row, col;
    xy_to_row_col(x, y, &row, &col);
    return row_col_to_cell(row, col);
}
