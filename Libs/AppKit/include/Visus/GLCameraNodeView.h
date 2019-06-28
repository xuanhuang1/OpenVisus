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

#ifndef VISUS_GLCAMERA_NODE_VIEW_H__
#define VISUS_GLCAMERA_NODE_VIEW_H__

#include <Visus/AppKit.h>
#include <Visus/GLCameraNode.h>
#include <Visus/Model.h>
#include <Visus/GuiFactory.h>



namespace Visus {

/////////////////////////////////////////////////////////
class VISUS_APPKIT_API GLCameraNodeView : 
  public QFrame, 
  public View<GLCameraNode>
{
public:

  VISUS_NON_COPYABLE_CLASS(GLCameraNodeView)

  //constructor
  GLCameraNodeView(GLCameraNode* model) {
    bindModel(model);
  }

  //destructor
  virtual ~GLCameraNodeView() {
    bindModel(nullptr);
  }

  //bindModel
  virtual void bindModel(GLCameraNode* model) override {

    if (this->model)
      QUtils::clearQWidget(this);

    View<ModelClass>::bindModel(model);

    if (this->model)
    {
      QFormLayout* layout=new QFormLayout();

      {
        auto sub=new QVBoxLayout();
        sub->addWidget(widgets.pos.x=new QLabel());
        sub->addWidget(widgets.pos.y=new QLabel());
        sub->addWidget(widgets.pos.z=new QLabel());
        layout->addRow("Position",sub);
      }

      {
        auto sub=new QVBoxLayout();
        sub->addWidget(widgets.dir.x=new QLabel());
        sub->addWidget(widgets.dir.y=new QLabel());
        sub->addWidget(widgets.dir.z=new QLabel());
        layout->addRow("Direction",sub);
      }

      {
        auto sub=new QVBoxLayout();
        sub->addWidget(widgets.vup.x=new QLabel());
        sub->addWidget(widgets.vup.y=new QLabel());
        sub->addWidget(widgets.vup.z=new QLabel());
        layout->addRow("View Up",sub);
      }

      {
        auto sub=new QVBoxLayout();
        sub->addWidget(widgets.ortho_params.left=new QLabel());
        sub->addWidget(widgets.ortho_params.right=new QLabel());
        sub->addWidget(widgets.ortho_params.bottom=new QLabel());
        sub->addWidget(widgets.ortho_params.top=new QLabel());
        layout->addRow("Ortho Params",sub);
      }

      setLayout(layout);

      refreshGui();
    }  
  }

private:

  class Widgets
  {
  public:
    struct
    {
      QLabel *x=nullptr,*y=nullptr,*z=nullptr;
    }
    pos,dir,vup;

    struct
    {
      QLabel *left=nullptr,*right=nullptr,*top=nullptr,*bottom=nullptr;
    }
    ortho_params;
  };

  Widgets widgets;

  //refreshGui
  void refreshGui()
  {
    auto camera=model->getGLCamera();
    Point3d pos,dir,vup;
    camera->getFrustum().getModelview().getLookAt(pos,dir,vup);

    widgets.pos.x->setText(std::to_string(pos[0]).c_str());
    widgets.pos.y->setText(std::to_string(pos[1]).c_str());
    widgets.pos.z->setText(std::to_string(pos[2]).c_str());

    widgets.dir.x->setText(std::to_string(dir[0]).c_str());
    widgets.dir.y->setText(std::to_string(dir[1]).c_str());
    widgets.dir.z->setText(std::to_string(dir[2]).c_str());

    widgets.vup.x->setText(std::to_string(vup[0]).c_str());
    widgets.vup.y->setText(std::to_string(vup[1]).c_str());
    widgets.vup.z->setText(std::to_string(vup[2]).c_str());

    auto ortho_params=camera->getOrthoParams();

    widgets.ortho_params.left  ->setText(std::to_string(ortho_params.left  ).c_str());
    widgets.ortho_params.right ->setText(std::to_string(ortho_params.right ).c_str());
    widgets.ortho_params.top   ->setText(std::to_string(ortho_params.top   ).c_str());
    widgets.ortho_params.bottom->setText(std::to_string(ortho_params.bottom).c_str());
  }

  //modelChanged
  virtual void modelChanged() override {
    refreshGui();
  }

};

} //namespace Visus

#endif //VISUS_GLCAMERA_NODE_VIEW_H__


