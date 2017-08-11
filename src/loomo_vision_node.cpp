//
// Created by kai on 17-7-20.
//

#include <stdio.h>
#include <string>

#include "ros/ros.h"

#include <message_filters/subscriber.h>
#include <message_filters/synchronizer.h>
#include "message_filters/subscriber.h"
#include "message_filters/time_synchronizer.h"
#include "message_filters/sync_policies/approximate_time.h"

#include "image_transport/subscriber_filter.h"

#include "sensor_msgs/Image.h"
#include "sensor_msgs/CameraInfo.h"
#include "sensor_msgs/PointCloud.h"
#include "sensor_msgs/PointCloud2.h"

using namespace std;
using namespace sensor_msgs;

typedef message_filters::sync_policies::ApproximateTime<
        sensor_msgs::Image, sensor_msgs::CameraInfo, sensor_msgs::Image, sensor_msgs::CameraInfo>
MySyncPolicy;

static string gRsTopicPrefix = "/loomo/realsense";

//std::vector<Eigen::Vector3f> depthImage2PointClouds(cv::Mat depthImage){
//    const float camera_scalar = 1000;
//    int rows = depthImage.rows;
//    int cols = depthImage.cols;
//    cv::Mat depth = depthImage.clone();
//    float constant_x = 1.0f / (camera_scalar * camera.fx);
//    float constant_y = 1.0f / (camera_scalar * camera.fy);
//    std::vector<Eigen::Vector3f> depthPoints;
//    depthPoints.clear();
//
//    for (int r = 0; r < rows; r++) {
//        unsigned short* data = depthImage.ptr<ushort>(r);
//        for (int c = 0; c < cols; c += 2) {
//            unsigned short z = data[c];
//            // check if depth data available at this pixel
//            if (z == 0)
//            {
//                continue;
//            }
//
//            Eigen::Vector4f p;
//            Eigen::Vector3f dst_p;
//            float projection_ratio1 = z * constant_x;
//            float projection_ratio2 = z * constant_y;
//            p[0] = (c - camera.cx) * projection_ratio1;
//            p[1] = (r - camera.cy) * projection_ratio2;
//            p[2] = (z) / camera_scalar;
//            p[3] = 1;
//
//            dst_p = p;
//
//            depthPoints.push_back(dst_p);
//
//        }
//    }
//    return depthPoints;
//}

PointCloudPtr depth2PointCloud(const ImageConstPtr& depth, const CameraInfoConstPtr& depthInfo) {
    const float camera_scalar = 1000;
    PointCloudPtr pcptr(new PointCloud());

    pcptr->points.resize(depth->data.size());
    pcptr->header = depth->header;

    int width = depthInfo->width, height = depthInfo->height;
    float fx = depthInfo->K[0], fy = depthInfo->K[4], cx = depthInfo->K[2], cy = depthInfo->K[5];
    float constant_x = 1.0f / (camera_scalar * fx);
    float constant_y = 1.0f / (camera_scalar * fy);

    for (int y = 0; y < height; ++y) {
        const unsigned short* data = (const unsigned short*)&depth->data[y * width];
        geometry_msgs::Point32* target = &pcptr->points[y * width];
        for (int x = 0; x < width; ++x) {
            unsigned short z = data[x];
            if (!z)
                continue;
            target[x].x = (x - cx) * z * constant_x;
            target[x].y = (y - cy) * z * constant_y;
            target[x].z = z / camera_scalar;
        }
    }
    return pcptr;
}

void syncCallback(const sensor_msgs::ImageConstPtr& color, const sensor_msgs::CameraInfoConstPtr& colorInfo, \
                    const sensor_msgs::ImageConstPtr& depth, const sensor_msgs::CameraInfoConstPtr& depthInfo) {
    // synced based on approximate time
    PointCloudPtr pcptr = depth2PointCloud(depth, depthInfo);
    printf("synced image callback {color,colorInfo,depth,depthInfo}[%.3lf,%.3lf,%.3lf,%.3lf]\n", \
            color->header.stamp.toSec(), colorInfo->header.stamp.toSec(), \
            depth->header.stamp.toSec(), depthInfo->header.stamp.toSec());
}

class VisualizerRepubr
{
public:
    // relative topic names (relative to topic_base)
    static string RsColorTopic, RsColorInfoTopic, RsDepthTopic, RsDepthInfoTopic;

    VisualizerRepubr(ros::NodeHandle& nh, const string& topic_base): \
            mNhPtr(nh), mIt(nh), mRsColorSubr(mIt, topic_base + RsColorTopic, 3), mRsDepthSubr(mIt, topic_base + RsDepthTopic, 3), \
            mRsColorInfoSubr(nh, topic_base + RsColorInfoTopic, 3), mRsDepthInfoSubr(nh, topic_base + RsDepthInfoTopic, 3), \
            mSync4(MySyncPolicy(5), mRsColorSubr, mRsColorInfoSubr, mRsDepthSubr, mRsDepthInfoSubr){

        printf("Initialized image transport::SubscriberFilter[%s,%s], CameraInfo subscriber[%s,%s]\n", mRsColorSubr.getTopic().c_str(), \
                mRsDepthSubr.getTopic().c_str(), mRsColorInfoSubr.getTopic().c_str(), mRsDepthInfoSubr.getTopic().c_str());
        mSync4.registerCallback(boost::bind(&VisualizerRepubr::sync4Callback, this, _1, _2, _3, _4));
    }

protected:

    void sync4Callback(const sensor_msgs::ImageConstPtr& color, const sensor_msgs::CameraInfoConstPtr& colorInfo, \
                    const sensor_msgs::ImageConstPtr& depth, const sensor_msgs::CameraInfoConstPtr& depthInfo) {
        printf("synced image callback {color,colorInfo,depth,depthInfo}[%.3lf,%.3lf,%.3lf,%.3lf]\n", \
            color->header.stamp.toSec(), colorInfo->header.stamp.toSec(), \
            depth->header.stamp.toSec(), depthInfo->header.stamp.toSec());
        PointCloudPtr pcptr = depth2PointCloud(depth, depthInfo);
    }

private:
    ros::NodeHandle mNhPtr;
    image_transport::ImageTransport mIt;
    image_transport::SubscriberFilter mRsColorSubr, mRsDepthSubr;
    message_filters::Subscriber<CameraInfo> mRsColorInfoSubr, mRsDepthInfoSubr;

    // synced RS 4 images based on approximate time
    message_filters::Synchronizer<MySyncPolicy> mSync4;
};

// default values for these topic names
string VisualizerRepubr::RsColorTopic = "/rgb";
string VisualizerRepubr::RsColorInfoTopic = "/rgb/camera_info";
string VisualizerRepubr::RsDepthTopic = "/depth";
string VisualizerRepubr::RsDepthInfoTopic = "/depth/camera_info";


int main(int argc, char **argv) {
    ros::init(argc, argv, "loomo_vision_node");

    ros::NodeHandle nh;

    VisualizerRepubr repubr(nh, gRsTopicPrefix);

//    image_transport::ImageTransport it(nh);
//
//    image_transport::SubscriberFilter colorSubr(it, "/loomo/realsense/rgb/compressed", 3);
//    image_transport::SubscriberFilter depthSubr(it, "/loomo/realsense/depth", 3);
//    message_filters::Subscriber<CameraInfo> colorInfoSubr(nh, "/loomo/realsense/rgb/camera_info", 3);
//    message_filters::Subscriber<CameraInfo> depthInfoSubr(nh, "/loomo/realsense/depth/camera_info", 3);
//
//    message_filters::Synchronizer<MySyncPolicy> sync(MySyncPolicy(5), colorSubr, colorInfoSubr, depthSubr, depthInfoSubr);
//    sync.registerCallback(boost::bind(&syncCallback, _1, _2, _3, _4));

    ros::spin();

    return 0;
}
