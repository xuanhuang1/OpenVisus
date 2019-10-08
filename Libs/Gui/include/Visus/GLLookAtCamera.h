/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_GL_LOOKAT_CAMERA_H
#define VISUS_GL_LOOKAT_CAMERA_H

#include <Visus/GLCamera.h>
#include <Visus/GLOrthoParams.h>
#include <Visus/GLMouse.h>

namespace Visus {

/////////////////////////////////////////////////////////////////
class VISUS_GUI_API GLLookAtCamera : public GLCamera
{
public:

  VISUS_NON_COPYABLE_CLASS(GLLookAtCamera)

  //constructor
  GLLookAtCamera(){
  }

  //destructor
  virtual ~GLLookAtCamera() {
  }

  //executeAction
  virtual void executeAction(StringTree in) override;

  //getTypeName
  virtual String getTypeName() const override {
    return "GLLookAtCamera";
  }

  //setBounds
  void setBounds(BoxNd value) {
    setProperty("bounds", this->bounds, value);
  }

  //guessPosition
  virtual bool guessPosition(BoxNd value,int ref=-1) override;

  //splitProjectionFrustum
  virtual void splitProjectionFrustum(Rectangle2d value) override {
    setProperty("split_projection_frustum", this->split_projection_frustum, value);
  }

  //getPos
  Point3d getPos() const {
    return pos;
  }

  //setPos
  void setPos(Point3d value) {
    setProperty("pos", this->pos, value);
  }

  //getDir
  Point3d getDir() const {
    return dir;
  }

  //setDir
  void setDir(Point3d value) {
    setProperty("dir", this->dir, value);
  }

  //getVup
  Point3d getVup() const {
    return vup;
  }

  //setVup
  void setVup(Point3d value) {
    setProperty("vup", this->vup, value);
  }

  //getLookAt
  virtual void getLookAt(Point3d& pos, Point3d& dir, Point3d& vup) const override {
    pos = getPos();
    dir = getDir();
    vup = getVup();
  }

  //getRotation
  Quaternion getRotation() const {
    return rotation;
  }

  //setRotation
  void setRotation(Quaternion value) {
    setProperty("rotation", this->rotation, value);
  }

  //getRotationCenter
  Point3d getRotationCenter() {
    return rotation_center;
  }

  //setRotationCenter
  void setRotationCenter(Point3d value) {
    setProperty("rotation_center", this->rotation_center, value);
  }

  //getFov
  double getFov() const {
    return fov;
  }

  //setFov
  void setFov(double value) {
    setProperty("fov", this->fov, fov);
  }

public:

  //getCurrentFrustum
  virtual Frustum getFinalFrustum(const Viewport& viewport) const override;

  //getCurrentFrustum
  virtual Frustum getCurrentFrustum(const Viewport& viewport) const override {
    return getFinalFrustum(viewport);
  }

  //glMousePressEvent
  virtual void glMousePressEvent(QMouseEvent* evt, const Viewport& viewport) override;

  //glMouseMoveEvent
  virtual void glMouseMoveEvent(QMouseEvent* evt, const Viewport& viewport) override;

  //glMouseReleaseEvent
  virtual void glMouseReleaseEvent(QMouseEvent* evt, const Viewport& viewport) override;

  //glWheelEvent
  virtual void glWheelEvent(QWheelEvent* evt, const Viewport& viewport) override;

  //glKeyPressEvent
  virtual void glKeyPressEvent(QKeyEvent* evt, const Viewport& viewport) override;


public:

  //writeTo
  virtual void writeTo(StringTree& out) const override;

  //readFrom
  virtual void readFrom(StringTree& in) override;

private:

  std::vector<Point2i>   last_mouse_pos = std::vector<Point2i>(GLMouse::getNumberOfButtons());
  GLMouse                mouse;

  BoxNd                  bounds = BoxNd(3);

  //projection
  double                 fov = 60.0;
  Rectangle2d            split_projection_frustum = Rectangle2d(0, 0, 1, 1);

  //modelview
  Point3d                pos, dir, vup;
  Quaternion             rotation;
  Point3d                rotation_center;

  //properties
  const double           rotation_factor=5.2;
  const double           pan_factor=30;
  const bool             disable_rotation = false;

  //guessForwardFactor
  double guessForwardFactor() const ;

  //guessNearFar
  std::pair<double,double> guessNearFar() const;


};//end class

} //namespace Visus

#endif //VISUS_GL_LOOKAT_CAMERA_H

