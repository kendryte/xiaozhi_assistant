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
#include "vi_vo.h"
#include "face_detection.h"

extern float kAnchors320[4200][4];
extern float kAnchors640[16800][4];
static float (*g_anchors)[4];

cv::Scalar color_list_for_det[] = {
    cv::Scalar(0, 0, 255),
    cv::Scalar(0, 255, 255),
    cv::Scalar(255, 0, 255),
    cv::Scalar(0, 255, 0),
    cv::Scalar(255, 0, 0)
};

cv::Scalar color_list_for_osd_det[] = {
    cv::Scalar(0, 0, 255, 255),
    cv::Scalar(0, 255, 255, 255),
    cv::Scalar(255, 0, 255, 255),
    cv::Scalar(0, 255, 0, 255),
    cv::Scalar(255, 0, 0, 255)
};

int nms_comparator(const void *pa, const void *pb)
{
    NMSRoiObj a = *(NMSRoiObj *)pa;
    NMSRoiObj b = *(NMSRoiObj *)pb;
    float diff = a.confidence - b.confidence;

    if (diff < 0)
        return 1;
    else if (diff > 0)
        return -1;
    return 0;
}

FaceDetection::FaceDetection(const char *kmodel_file, float obj_thresh,float nms_thresh, FrameCHWSize isp_shape, const int debug_mode) : obj_thresh_(obj_thresh), AIBase(kmodel_file,"FaceDetection", debug_mode)
{
    model_name_ = "FaceDetection";
    nms_thresh_ = nms_thresh;
    int net_len = input_shapes_[0][2];
    min_size_ = (net_len == 320 ? 200 : 800);
    g_anchors = (net_len == 320 ? kAnchors320 : kAnchors640);
    objs_num_ = min_size_ * (1 + 4 + 16);

    so_ = new NMSRoiObj[objs_num_];
    boxes_ = new float[objs_num_ * LOC_SIZE];
    landmarks_ = new float[objs_num_ * LAND_SIZE];

    // ai2d_in_tensor to isp
    isp_shape_ = isp_shape;
    dims_t in_shape{1, isp_shape.channel, isp_shape.height, isp_shape.width};

    ai2d_out_tensor_ = get_input_tensor(0);
    // set padding resize param
    Utils::padding_resize_one_side(isp_shape, {input_shapes_[0][3], input_shapes_[0][2]}, ai2d_builder_, ai2d_in_tensor_, ai2d_out_tensor_, cv::Scalar(104, 117, 123));
}

FaceDetection::~FaceDetection()
{
    ScopedTiming st(model_name_ + " ~FaceDetection", debug_mode_);
    delete[] so_;
    delete[] boxes_;
    delete[] landmarks_;
}

// ai2d for video
void FaceDetection::pre_process(runtime_tensor& img_data)
{
    ScopedTiming st(model_name_ + " pre_process video", debug_mode_);
    ai2d_builder_->invoke(img_data, ai2d_out_tensor_).expect("error occurred in ai2d running");
}

void FaceDetection::inference()
{
    this->run();
    this->get_output();
}

void FaceDetection::post_process(FrameSize frame_size, vector<FaceDetectionInfo> &results)
{
    ScopedTiming st(model_name_ + " post_process", debug_mode_);
    int obj_cnt = 0;
    deal_conf(p_outputs_[3], so_, 16 * min_size_ / 2, obj_cnt);
    deal_conf(p_outputs_[4], so_, 4 * min_size_ / 2, obj_cnt);
    deal_conf(p_outputs_[5], so_, 1 * min_size_ / 2, obj_cnt);
    obj_cnt = 0;
    deal_loc(p_outputs_[0], boxes_, 16 * min_size_ / 2, obj_cnt);
    deal_loc(p_outputs_[1], boxes_, 4 * min_size_ / 2, obj_cnt);
    deal_loc(p_outputs_[2], boxes_, 1 * min_size_ / 2, obj_cnt);
    obj_cnt = 0;
    deal_landms(p_outputs_[6], landmarks_, 16 * min_size_ / 2, obj_cnt);
    deal_landms(p_outputs_[7], landmarks_, 4 * min_size_ / 2, obj_cnt);
    deal_landms(p_outputs_[8], landmarks_, 1 * min_size_ / 2, obj_cnt);
    qsort(so_, objs_num_, sizeof(NMSRoiObj), nms_comparator);
    get_final_box(frame_size, results);
}

void FaceDetection::get_final_box(FrameSize &frame_size, vector<FaceDetectionInfo> &results)
{
    int iou_cal_times = 0;
    int i, j, obj_index;
    for (i = 0; i < objs_num_; ++i)
    {
        obj_index = so_[i].index;
        if (so_[i].confidence < obj_thresh_)
            continue;
        FaceDetectionInfo obj;
        obj.bbox = get_box(boxes_, obj_index);
        obj.sparse_kps = get_landmark(landmarks_, obj_index);

        for (j = i + 1; j < objs_num_; ++j)
        {
            obj_index = so_[j].index;
            if (so_[j].confidence < obj_thresh_)
                continue;
            Bbox b = get_box(boxes_, obj_index);
            iou_cal_times += 1;
            if (box_iou(obj.bbox, b) >= nms_thresh_) // thres
                so_[j].confidence = 0;
        }
        obj.score = so_[i].confidence;
        results.push_back(obj);
    }

    int max_src_size = std::max(frame_size.width, frame_size.height);
    for (int i = 0; i < results.size(); ++i)
    {
        auto &l = results[i].sparse_kps;
        for (uint32_t ll = 0; ll < 5; ll++)
        {
            l.points[2 * ll + 0] = l.points[2 * ll + 0] * max_src_size;
            l.points[2 * ll + 1] = l.points[2 * ll + 1] * max_src_size;
        }

        auto &b = results[i].bbox;
        float x1 = (b.x + b.w / 2) * max_src_size;
        float x0 = (b.x - b.w / 2) * max_src_size;
        float y0 = (b.y - b.h / 2) * max_src_size;
        float y1 = (b.y + b.h / 2) * max_src_size;
        x1 = std::max(float(0), std::min(x1, float(frame_size.width)));
        x0 = std::max(float(0), std::min(x0, float(frame_size.width)));
        y0 = std::max(float(0), std::min(y0, float(frame_size.height)));
        y1 = std::max(float(0), std::min(y1, float(frame_size.height)));
        b.x = x0;
        b.y = y0;
        b.w = x1 - x0;
        b.h = y1 - y0;
    }
}

void FaceDetection::local_softmax(float *x, float *dx, uint32_t len)
{
    float max_value = x[0];
    for (uint32_t i = 0; i < len; i++)
    {
        if (max_value < x[i])
        {
            max_value = x[i];
        }
    }
    for (uint32_t i = 0; i < len; i++)
    {
        x[i] -= max_value;
        x[i] = expf(x[i]);
    }
    float sum_value = 0.0f;
    for (uint32_t i = 0; i < len; i++)
    {
        sum_value += x[i];
    }
    for (uint32_t i = 0; i < len; i++)
    {
        dx[i] = x[i] / sum_value;
    }
}

void FaceDetection::deal_conf(float *conf, NMSRoiObj *so, int size, int &obj_cnt)
{
    float confidence[CONF_SIZE] = {0.0};
    for (uint32_t ww = 0; ww < size; ww++)
    {
        for (uint32_t hh = 0; hh < 2; hh++)
        {
            for (uint32_t cc = 0; cc < CONF_SIZE; cc++)
            {
                confidence[cc] = conf[(hh * CONF_SIZE + cc) * size + ww];
            }
            local_softmax(confidence, confidence, 2);
            so_[obj_cnt].index = obj_cnt;
            so_[obj_cnt].confidence = confidence[1];
            obj_cnt += 1;
        }
    }
}

void FaceDetection::deal_loc(float *loc, float *boxes, int size, int &obj_cnt)
{
    for (uint32_t ww = 0; ww < size; ww++)
    {
        for (uint32_t hh = 0; hh < 2; hh++)
        {
            for (uint32_t cc = 0; cc < LOC_SIZE; cc++)
            {
                boxes_[obj_cnt * LOC_SIZE + cc] = loc[(hh * LOC_SIZE + cc) * size + ww];
            }
            obj_cnt += 1;
        }
    }
}

void FaceDetection::deal_landms(float *landms, float *landmarks, int size, int &obj_cnt)
{
    // chw->hwc
    for (uint32_t ww = 0; ww < size; ww++)
    {
        for (uint32_t hh = 0; hh < 2; hh++)
        {
            for (uint32_t cc = 0; cc < LAND_SIZE; cc++)
            {
                landmarks_[obj_cnt * LAND_SIZE + cc] = landms[(hh * LAND_SIZE + cc) * size + ww];
            }
            obj_cnt += 1;
        }
    }
}

Bbox FaceDetection::get_box(float *boxes, int obj_index)
{
    float cx, cy, w, h;
    cx = boxes_[obj_index * LOC_SIZE + 0];
    cy = boxes_[obj_index * LOC_SIZE + 1];
    w = boxes_[obj_index * LOC_SIZE + 2];
    h = boxes_[obj_index * LOC_SIZE + 3];
    cx = g_anchors[obj_index][0] + cx * 0.1 * g_anchors[obj_index][2];
    cy = g_anchors[obj_index][1] + cy * 0.1 * g_anchors[obj_index][3];
    w = g_anchors[obj_index][2] * expf(w * 0.2);
    h = g_anchors[obj_index][3] * expf(h * 0.2);
    Bbox box;
    box.x = cx;
    box.y = cy;
    box.w = w;
    box.h = h;
    return box;
}

SparseLandmarks FaceDetection::get_landmark(float *landmarks, int obj_index)
{
    SparseLandmarks landmark;
    for (uint32_t ll = 0; ll < 5; ll++)
    {
        landmark.points[2 * ll + 0] = g_anchors[obj_index][0] + landmarks_[obj_index * LAND_SIZE + 2 * ll + 0] * 0.1 * g_anchors[obj_index][2];
        landmark.points[2 * ll + 1] = g_anchors[obj_index][1] + landmarks_[obj_index * LAND_SIZE + 2 * ll + 1] * 0.1 * g_anchors[obj_index][3];
    }
    return landmark;
}

float FaceDetection::overlap(float x1, float w1, float x2, float w2)
{
    float l1 = x1 - w1 / 2;
    float l2 = x2 - w2 / 2;
    float left = l1 > l2 ? l1 : l2;
    float r1 = x1 + w1 / 2;
    float r2 = x2 + w2 / 2;
    float right = r1 < r2 ? r1 : r2;
    return right - left;
}

float FaceDetection::box_intersection(Bbox a, Bbox b)
{
    float w = overlap(a.x, a.w, b.x, b.w);
    float h = overlap(a.y, a.h, b.y, b.h);

    if (w < 0 || h < 0)
        return 0;
    return w * h;
}

float FaceDetection::box_union(Bbox a, Bbox b)
{
    float i = box_intersection(a, b);
    float u = a.w * a.h + b.w * b.h - i;

    return u;
}

float FaceDetection::box_iou(Bbox a, Bbox b)
{
    return box_intersection(a, b) / box_union(a, b);
}


