// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2025, Canaan Bright Sight Co., Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _FACE_DETECTION_H
#define _FACE_DETECTION_H

#include <iostream>
#include <vector>
#include "utils.h"
#include "ai_base.h"

using std::vector;

#define LOC_SIZE 4
#define CONF_SIZE 2
#define LAND_SIZE 10
#define PI 3.1415926

/**
 * @brief 人脸五官点
 */
typedef struct SparseLandmarks
{
    float points[10]; // 人脸五官点,依次是图片的左眼（x,y）、右眼（x,y）,鼻子（x,y）,左嘴角（x,y）,右嘴角
} SparseLandmarks;

/**
 * @brief 用于NMS排序的roi对象
 */
typedef struct NMSRoiObj
{
    int index;        // roi对象所在原列表的索引
    float confidence; // roi对象的置信度
} NMSRoiObj;

/**
 * @brief 预测人脸roi信息
 */
typedef struct FaceDetectionInfo
{
    Bbox bbox;                  // 人脸检测框
    SparseLandmarks sparse_kps; // 人脸五官关键点
    float score;                // 人脸检测框置信度
} FaceDetectionInfo;

/**
 * @brief 人脸检测
 * 主要封装了对于每一帧图片，从预处理、运行到后处理给出结果的过程
 */
class FaceDetection : public AIBase
{
public:
    /**
     * @brief FaceDetection构造函数，加载kmodel,并初始化kmodel输入、输出和人脸检测阈值
     * @param kmodel_file kmodel文件路径
     * @param obj_thresh  人脸检测阈值，用于过滤roi
     * @param nms_thresh  人脸检测nms阈值
     * @param isp_shape   isp输入大小（chw）
     * @param debug_mode  0（不调试）、 1（只显示时间）、2（显示所有打印信息）
     * @return None
     */
    FaceDetection(const char *kmodel_file, float obj_thresh,float nms_thresh, FrameCHWSize isp_shape, const int debug_mode);

    /**
     * @brief FaceDetection析构函数
     * @return None
     */
    ~FaceDetection();

    /**
     * @brief 视频流预处理（ai2d for isp）
     * @param img_data 当前视频帧数据
     * @return None
     */
    void pre_process(runtime_tensor& img_data);

    /**
     * @brief kmodel推理
     * @return None
     */
    void inference();

    /**
     * @brief kmodel推理结果后处理
     * @param frame_size 原始图像/帧宽高，用于将结果放到原始图像大小
     * @param results 后处理之后的基于原始图像的{检测框、五官点和得分}集合
     * @return None
     */
    void post_process(FrameSize frame_size, vector<FaceDetectionInfo> &results);

private:
    /**
     * @brief softmax操作
     * @param x 需要处理数据指针
     * @param dx 处理数据后的指针
     * @param len 需要处理数据长度
     * @return None
     */
    void local_softmax(float *x, float *dx, uint32_t len);

    /**
     * @brief roi置信度后处理
     * @param conf    指向模型推理得到的首个roi置信度的指针
     * @param so      指向经过softmax之后首个roi置信度的指针
     * @param size    需要处理roi个数
     * @param obj_cnt 已经处理的roi个数，deal_conf会被调用多次
     * @return None
     */
    void deal_conf(float *conf, NMSRoiObj *so, int size, int &obj_cnt);

    /**
     * @brief roi位置（location）后处理
     * @param loc     模型推理后，指向首个roi位置的指针
     * @param boxes   经过NCHW-NHWC之后，指向首个roi位置的指针
     * @param size    需要处理roi个数
     * @param obj_cnt 已经处理的roi个数，deal_loc会被调用多次
     * @return None
     */
    void deal_loc(float *loc, float *boxes, int size, int &obj_cnt);

    /**
     * @brief roi五官关键点后处理
     * @param landms  模型推理后，指向首个roi对应的五官关键点的指针
     * @param boxes   经过NCHW-NHWC之后，指向首个roi对应的五官关键点的指针
     * @param size    需要处理roi个数
     * @param obj_cnt 已经处理的roi个数，deal_landms会被调用多次
     * @return None
     */
    void deal_landms(float *landms, float *landmarks, int size, int &obj_cnt);

    /**
     * @brief 根据索引值得到单个roi检测框
     * @param boxes       指向首个roi的检测框的指针
     * @param obj_index   需要获取的roi索引
     * @return None
     */
    Bbox get_box(float *boxes, int obj_index);

    /**
     * @brief 根据索引值得到单个roi对应的五官关键点
     * @param landmarks   指向首个roi对应的五官关键点指针
     * @param obj_index   需要获取的roi索引
     * @return None
     */
    SparseLandmarks get_landmark(float *landmarks, int obj_index);

    /**
     * @brief 获取2个检测框重叠区域的左上角或右下角
     * @param x1   第1个检测框的中心点x或y坐标
     * @param w1   第1个检测框的宽（w）或高（h）
     * @param x2   第2个检测框的中心点x或y坐标
     * @param w2   第2个检测框的宽（w）或高（h）
     * @return 2个检测框重叠区域的宽或高
     */
    float overlap(float x1, float w1, float x2, float w2);

    /**
     * @brief 获取2个检测框重叠区域的面积
     * @param a   第1个检测框
     * @param b   第2个检测框
     * @return 2个检测框重叠区域的面积
     */
    float box_intersection(Bbox a, Bbox b);

    /**
     * @brief 获取2个检测框联合区域的面积
     * @param a   第1个检测框
     * @param b   第2个检测框
     * @return 2个检测框重叠联合区域的面积
     */
    float box_union(Bbox a, Bbox b);

    /**
     * @brief 获取2个检测框的iou
     * @param a   第1个检测框
     * @param b   第2个检测框
     * @return 2个检测框的iou
     */
    float box_iou(Bbox a, Bbox b);

    /**
     * @brief 获取2个检测框的iou
     * @param frame_size  原始图像/帧宽高，用于将结果放到原始图像大小
     * @param results     后处理之后的基于原始图像的{检测框、五官点和得分}集合
     * @return None
     */
    void get_final_box(FrameSize &frame_size, vector<FaceDetectionInfo> &results);

    std::unique_ptr<ai2d_builder> ai2d_builder_; // ai2d构建器
    runtime_tensor ai2d_in_tensor_;             // ai2d输入tensor
    runtime_tensor ai2d_out_tensor_;             // ai2d输出tensor
    FrameCHWSize isp_shape_;                     // isp对应的地址大小

    int min_size_;
    float obj_thresh_; // 人脸检测阈值
    float nms_thresh_; // nms阈值
    int objs_num_;     // roi个数

    NMSRoiObj *so_;    // 指向经过softmax之后首个roi置信度的指针
    float *boxes_;     // 指向首个roi检测框的指针
    float *landmarks_; // 指向首个roi对应五官关键点的指针
};

#endif