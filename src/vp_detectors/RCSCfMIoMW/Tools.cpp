#include "Tools.hpp"
#include <VanishingPointDetectionTools.hpp>

#include <iostream>
#include <cmath>
#include <limits>
#include <time.h>

namespace vanishing_point {

cv::Point2f lineSegmentCenterPoint(cv::Vec4f line_segment){
  double center_x = (line_segment[0]+line_segment[2])/2;
  double center_y = (line_segment[1]+line_segment[3])/2;
  return cv::Point2f(center_x, center_y);
};

double distancePoint2Line(cv::Point3f line, cv::Point2f point){
  double distance = line.x*point.x + line.y*point.y + line.z;
  return fabs(distance)/(sqrt(line.x*line.x + line.y*line.y));
}

double errorLineSegmentPoint2VP(  cv::Vec4f line_segment,
                                  cv::Point3f homogeneo_vp,
                                  cv::Point2f *center_point,
                                  cv::Point2f *end_point,
                                  cv::Point3f *line_center_vp){

  cv::Point2f local_center_point = lineSegmentCenterPoint(line_segment);
  cv::Point2f vp( homogeneo_vp.x/homogeneo_vp.z,
                  homogeneo_vp.y/homogeneo_vp.z);

  cv::Point2f local_end_point(line_segment[0],line_segment[1]);
  cv::Point3f line = defineEuclidianLineBy2Points(local_center_point, vp);
  if(center_point && end_point && line_center_vp){

  }
  return distancePoint2Line(line, local_end_point);
}

std::vector<cv::Point2f> estimationVPby4LinesCase1(
                                          std::vector<cv::Point3f> lines,
                                          double *focal_length){

  std::vector<cv::Point2f> vps(3);
  for (uint j = 0; j < 2; j++)
    vps[j] = definePointByEuclidianLinesIntersection( lines[j*2], lines[j*2+1]);

  double local_focal = -(vps[0].x*vps[1].x + vps[0].y*vps[1].y);
  if(local_focal <= 0){
    if(focal_length)
      (*focal_length) = -1;
    return std::vector<cv::Point2f>(3);
  }

  local_focal = sqrt(local_focal);

  cv::Mat1f mat_K = cv::Mat1f::zeros(3,3);
  mat_K[0][0] = local_focal;
  mat_K[1][1] = local_focal;
  mat_K[2][2] = 1;
  cv::Mat1f mat_K_inv = mat_K.inv(); // can optimeze

  cv::Point3f vp0(vps[0].x,vps[0].y,1);
  cv::Point3f vp1(vps[1].x,vps[1].y,1);

  cv::Mat1f vp2 = mat_K_inv * cv::Mat1f(vp0); // can optimeze
  vp2 = vp2.cross(mat_K_inv * cv::Mat1f(vp1));
  vp2 = mat_K * vp2;
  vps[2] = cv::Point2f(vp2[0][0]/vp2[0][2], vp2[0][1]/vp2[0][2]);

  if( std::isinf( vps[2].x) || std::isinf( vps[2].y)
      || std::isnan(vps[2].x) || std::isnan(vps[2].y) ){

    if(focal_length)
      (*focal_length) = -1;
    return std::vector<cv::Point2f>(3);
  }

  if(focal_length)
    (*focal_length) = local_focal;

  return vps;
}

std::vector<cv::Point2f> estimationVPby4LinesCase2(
                                          std::vector<cv::Point3f> lines,
                                          double *focal_length){
  std::vector<cv::Point2f> vps(3);
  vps[0] = definePointByEuclidianLinesIntersection(lines[0], lines[1]);

  cv::Point3f consts(0,0,0);

  consts.x =                        lines[2].x*lines[3].x
             +                      lines[2].y*lines[3].y;

  consts.y =   vps[0].y*  vps[0].y* lines[2].x*lines[3].x
             - vps[0].x*  vps[0].y* lines[2].y*lines[3].x
             - vps[0].x*            lines[2].z*lines[3].x
             - vps[0].x*  vps[0].y* lines[2].x*lines[3].y
             - vps[0].y*            lines[2].z*lines[3].y
             - vps[0].x*            lines[2].x*lines[3].z
             - vps[0].y*            lines[2].y*lines[3].z
             + vps[0].x*  vps[0].x* lines[2].y*lines[3].y;

  consts.z =   vps[0].x*            lines[2].z*lines[3].z
             + vps[0].y*  vps[0].y* lines[2].z*lines[3].z;

  double delta = (consts.y*consts.y)-4*consts.x*consts.z;

  if(delta < 0){
    if(focal_length)
      (*focal_length) = -1;
    return std::vector<cv::Point2f>(3);
  }

  delta = sqrt(delta);
  cv::Vec2f roots( ((+consts.y)+delta)/(2*consts.x),
                   ((-consts.y)+delta)/(2*consts.x));

  double focal_length_2;
  roots[0] > 0 ? focal_length_2 = roots[0] : focal_length_2 = roots[1];

  if(focal_length_2 < 0){
    if(focal_length)
      (*focal_length) = -1;
    return std::vector<cv::Point2f>(3);
  }

  cv::Point3f line_h( -vps[0].x/vps[0].y,
                      -1.0,
                      -1.0/(vps[0].y/focal_length_2));

  vps[1] = definePointByEuclidianLinesIntersection(line_h, lines[2]);
  vps[2] = definePointByEuclidianLinesIntersection(line_h, lines[3]);

  for (uint i = 1; i < 3; i++) {
    if( std::isinf( vps[i].x) || std::isinf( vps[i].y)
        || std::isnan(vps[i].x) || std::isnan(vps[i].y) ){

      if(focal_length)
        (*focal_length) = -1;
      return std::vector<cv::Point2f>(3);
    }
  }

  if(focal_length)
    (*focal_length) = sqrt(focal_length_2);

  return vps;
}

std::vector< std::vector<cv::Point2f> > estimationVPby4LinesInAll9Cases(
                                    std::vector<cv::Point3f> lines,
                                    std::vector<double> *vector_focal_length){

  std::vector< std::vector<cv::Point2f> > vector_vps(9);
  std::vector<double> vector_foco(9);

  // case 1 original configuration
  std::vector<cv::Point3f> temp_lines = lines;
  vector_vps[0] = estimationVPby4LinesCase1(temp_lines, &vector_foco[0]);

  // case 1 second configuration
  temp_lines = {lines[1], lines[2], lines[3], lines[0]};
  vector_vps[1] = estimationVPby4LinesCase1(temp_lines, &vector_foco[1]);

  // case 1 third configuration
  temp_lines = {lines[0], lines[2], lines[1], lines[3]};
  vector_vps[2] = estimationVPby4LinesCase1(temp_lines, &vector_foco[2]);

  // case 2 original configuration
  temp_lines = lines;
  vector_vps[3] = estimationVPby4LinesCase2(lines, &vector_foco[3]);

  // case 2 second configuration
  temp_lines = {lines[0], lines[2], lines[1], lines[3]};
  vector_vps[4] = estimationVPby4LinesCase2(temp_lines, &vector_foco[4]);

  // case 2 third configuration
  temp_lines = {lines[0], lines[3], lines[1], lines[2]};
  vector_vps[5] = estimationVPby4LinesCase2(temp_lines, &vector_foco[5]);

  // case 2 fourth configuration
  temp_lines = {lines[1], lines[2], lines[0], lines[3]};
  vector_vps[6] = estimationVPby4LinesCase2(temp_lines, &vector_foco[6]);

  // case 2 fifth configuration
  temp_lines = {lines[1], lines[3], lines[0], lines[2]};
  vector_vps[7] = estimationVPby4LinesCase2(temp_lines, &vector_foco[7]);

  // case 2 sixth configuration
  temp_lines = {lines[2], lines[3], lines[0], lines[1]};
  vector_vps[8] = estimationVPby4LinesCase2(temp_lines, &vector_foco[8]);

  if(vector_focal_length)
    (*vector_focal_length) = vector_foco;

  return vector_vps;
}

bool isPointLaySegmentLine(cv::Point2f point,
                           cv::Vec4f segment){

  double calc1 = (segment[2] - segment[0]) * (point.y - segment[1]);
  double calc2 = (point.x - segment[0]) * (segment[3] - segment[1]);

  int precision = 1e3;
  bool isCollinear = int(calc1*precision) == int(calc2*precision);

  bool isBetween = false;
  if(isCollinear)
    if(segment[0] != segment[2]){
      isBetween =     ((segment[0] <= point.x) && (point.x <= segment[2]))
                   || ((segment[2] <= point.x) && (point.x <= segment[0]));
    }else{
      isBetween =     ((segment[1] <= point.y) && (point.y <= segment[3]))
                   || ((segment[3] <= point.y) && (point.y <= segment[1]));
    }

  return isBetween;

}

std::vector< std::vector<cv::Point2f> > filterHypotheses(
                            std::vector< std::vector<cv::Point2f> > vps,
                            std::vector< double > &focos,
                            std::vector< cv::Vec4f > segments,
                            double threshold){

  std::vector< std::vector<cv::Point2f> > selected_vps;
  std::vector< double > selected_focos;
  for (uint i = 0; i < vps.size(); i++) {
    if(focos[i] < 0)
      continue;

    bool point_lay_segment = false;
    for (uint k = 0; !point_lay_segment && k < vps[i].size(); k++)
      for (uint j = 0; !point_lay_segment && j < segments.size()-1; j++)
        point_lay_segment = isPointLaySegmentLine(vps[i][k], segments[j]);

    if(point_lay_segment)
      continue;

    int count = 0;
    for (uint k = 0; k < vps[i].size(); k++){
      cv::Point3f homogeneos_vp(vps[i][k].x, vps[i][k].y, 1);
      for (uint j = 0; j < segments.size(); j++){
        double error_vp = errorLineSegmentPoint2VP(segments[j], homogeneos_vp);
        if(error_vp < threshold)
          count++;
      }
    }

    if(count != (segments.size() -1))
      continue;

    selected_vps.push_back(vps[i]);
    selected_focos.push_back(focos[i]);
  }

  focos = selected_focos;
  return selected_vps;
}


uint consensusSet(std::vector<cv::Point2f> vps,
                  std::vector<cv::Vec4f> segments,
                  std::vector<int> &lines_cluster,
                  double threshold){

  std::vector<int> cluster(segments.size(),-1);
  uint count = 0;
  for (uint i = 0; i < segments.size(); i++)
    for (uint j = 0; j < vps.size(); j++) {
      double error = errorLineSegmentPoint2VP(segments[i],
                                            cv::Point3f(vps[j].x, vps[j].y,1));
      if(error < threshold){
        count++;
        cluster[i] = j;
        break;
      }
    }
    lines_cluster = cluster;
    return count;
}

std::vector<cv::Point2f> RANSAC(std::vector<cv::Vec4f> segments,
                                std::vector<cv::Point3f> lines,
                                uint iterations,
                                double threshold){

  // random values
  std::time_t seconds;
  std::time(&seconds);
  std::srand((unsigned int) seconds);


  std::vector<int> line_cluster;
  uint count_final = 0;
  std::vector<int> selected_index(5);
  for (uint i = 0; i < iterations; i++) {
      for (uint j = 0; j < 5; j++)
        selected_index[j] = ( rand()%segments.size() );

      // std::cout << cv::Mat(selected_index).t() <<std::endl;
      std::vector<cv::Vec4f> local_segments(5);
      std::vector<cv::Point3f> local_lines(5);
      for (uint j = 0; j < segments.size(); j++) {
        local_segments[j] = segments[selected_index[j]];
        local_lines[j] = lines[selected_index[j]];
      }

      // generate hypotheses
      std::vector< double > temp_foco;
      std::vector< std::vector<cv::Point2f> > temp_vps;
      temp_vps = estimationVPby4LinesInAll9Cases(lines, &temp_foco);
      temp_vps = filterHypotheses(temp_vps, temp_foco, local_segments);

      // generate consensu set
      for (uint j = 0; j < temp_vps.size(); j++) {
        // std::vector<int> temp_cluster(lines.size(),-1);
        // uint count_temp = 0;
        // for (uint k = 0; k < lines.size(); k++)
        //   for (uint l = 0; l < temp_vps[j].size(); l++) {
        //     /* code */
        //   }


      }
  }

  return std::vector<cv::Point2f>();
}


}
