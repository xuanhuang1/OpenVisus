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

#include <Visus/QueryNode.h>
#include <Visus/DatasetFilter.h>
#include <Visus/Dataflow.h>
#include <Visus/StringTree.h>

namespace Visus {

///////////////////////////////////////////////////////////////////////////
class QueryNode::MyJob : public NodeJob
{
public:

  QueryNode*               node;
  SharedPtr<Dataset>       dataset;
  SharedPtr<Access>        access;

  Field                    field;
  double                   time;
  Position                 logic_position;
  std::vector<int>         resolutions;
  Frustum                  logic_to_screen;


  bool                     verbose;
  SharedPtr<Semaphore>     waiting_ready = std::make_shared<Semaphore>();

  //constructor
  MyJob(QueryNode* node_,SharedPtr<Dataset> dataset_,SharedPtr<Access> access_)
    : node(node_),dataset(dataset_),access(access_)
  {
    this->field = node->getField();
    this->time  = node->getTime();
    this->logic_position = node->getQueryLogicPosition();
    this->resolutions = dataset->guessEndResolutions(node->logicToScreen(), logic_position, node->getQuality(), node->getProgression());
    this->logic_to_screen = node->logicToScreen();
    this->verbose = node->isVerbose();
  }

  //destructor
  virtual ~MyJob()
  {
  }

  //runJob
  virtual void runJob() override
  {
    int pdim = dataset->getPointDim();

    if (bool bPointQuery = pdim == 3 && logic_position.getBoxNd().toBox3().minsize() == 0)
    {
      for (int N = 0; N < (int)this->resolutions.size(); N++)
      {
        Time t1 = Time::now();

        auto query = std::make_shared<PointQuery>(dataset.get(), field, time, 'r', this->aborted);
        query->logic_position = logic_position;
        query->end_resolution = this->resolutions[N];
        auto nsamples = dataset->guessPointQueryNumberOfSamples(logic_to_screen, logic_position, query->end_resolution);
        query->setPoints(nsamples);
        dataset->beginQuery(query);

        if (!dataset->executeQuery(access, query))
          return;
        
        auto output = query->buffer;

        if (verbose)
        {
          VisusInfo() << "PointQuery msec(" << t1.elapsedMsec() << ") " << "level(" << N << "/" << this->resolutions.size() << "/" << this->resolutions[N] << "/" << dataset->getMaxResolution() << ") "
            << "dims(" << output.dims.toString() << ") dtype(" << output.dtype.toString() << ") access(" << (access ? "yes" : "nullptr") << ") url(" << dataset->getUrl().toString() << ") ";
        }

        DataflowMessage msg;
        output.bounds = dataset->logicToPhysic(query->logic_position);
        msg.writeValue("data", output);
        node->publish(msg);
      }
    }
    else
    {
      auto query = std::make_shared<BoxQuery>(dataset.get(), field, time, 'r', this->aborted);
      query->filter.enabled = true;
      query->merge_mode = InsertSamples;
      query->logic_box = this->logic_position.toDiscreteAxisAlignedBox(); //remove transformation! (in doPublish I will add the physic clipping)
      query->end_resolutions = this->resolutions;

      query->incrementalPublish = [&](Array output) {
        doPublish(output, query);
      };

      dataset->beginQuery(query);

      //could be that end_resolutions gets corrected (see google maps for example)
      this->resolutions = query->end_resolutions; 

      for (int N = 0; N < (int)this->resolutions.size(); N++)
      {
        Time t1 = Time::now();

        if (aborted() || !query->isRunning() || !dataset->executeQuery(access, query) || aborted())
          return;

        auto output = query->buffer;

        if (verbose)
        {
          VisusInfo()<< "BoxQuery msec(" << t1.elapsedMsec() << ") "
            << "level(" << N << "/" << this->resolutions.size() << "/" << this->resolutions[N] << "/" << dataset->getMaxResolution() << ") "
            << "dims(" << output.dims.toString() << ") dtype(" << output.dtype.toString() << ") access(" << (access ? "yes" : "nullptr") << ") url(" << dataset->getUrl().toString() << ") ";
        }

        doPublish(output, query);
        dataset->nextQuery(query);
      }
    }
  }

  //doPublish
  void doPublish(Array output, SharedPtr<BoxQuery> query)
  {
    int pdim = dataset->getPointDim();

    if (auto filter= query->filter.dataset_filter)
      output = filter->dropExtraComponentIfExists(output);

    DataflowMessage msg;
    output.bounds = dataset->logicToPhysic(query->logic_box);

    if (pdim==3)
      output.clipping = dataset->logicToPhysic(this->logic_position);

    //a projection happened?
#if 1
    if (query->logic_samples.nsamples != output.dims)
    {
      //disable clipping
      output.clipping = Position::invalid();

      //fix bounds
      auto T   = output.bounds.getTransformation();
      auto box = output.bounds.getBoxNd();
      for (int D = 0; D < pdim; D++)
      {
        if (query->logic_samples.nsamples[D] > 1 && output.dims[D] == 1)
          box.p2[D] = box.p1[D];
      }
      output.bounds = Position(T, box);
    }
#endif

    msg.writeValue("data", output);
    node->publish(msg);
  }

  //abort
  virtual void abort() override
  {
    NodeJob::abort();
    waiting_ready->up(); //in case I'm in waitReady
  }

};


///////////////////////////////////////////////////////////////////////////
QueryNode::QueryNode(String name) : Node(name)
{
  addInputPort("dataset");
  addInputPort("fieldname");
  addInputPort("time");

  addOutputPort("data");
}

///////////////////////////////////////////////////////////////////////////
QueryNode::~QueryNode(){
}

///////////////////////////////////////////////////////////////////////////
Field QueryNode::getField()
{
  VisusAssert(VisusHasMessageLock());
  auto dataset = getDataset();
  if (!dataset)
    return Field();

  auto fieldname = readValue<String>("fieldname");
  return fieldname? dataset->getFieldByName(cstring(fieldname)) : dataset->getDefaultField();
}


///////////////////////////////////////////////////////////////////////////
double QueryNode::getTime()
{
  VisusAssert(VisusHasMessageLock());
  auto dataset = getDataset();
  if (!dataset)
    return 0.0;

  auto time = readValue<double>("time");
  return time ? cdouble(time) : dataset->getDefaultTime();
}


//////////////////////////////////////////////////////////////////
DatasetNode* QueryNode::getDatasetNode()
{
  VisusAssert(VisusHasMessageLock());
  if (!isInputConnected("dataset")) return nullptr;
  return dynamic_cast<DatasetNode*>((*getInputPort("dataset")->inputs.begin())->getNode());
}

//////////////////////////////////////////////////////////////////
SharedPtr<Dataset> QueryNode::getDataset() 
{
  VisusAssert(VisusHasMessageLock());
  return readValue<Dataset>("dataset");
}


///////////////////////////////////////////////////////////////////////////
Frustum QueryNode::logicToScreen()  
{
  auto dataset = getDataset();
  if (!dataset)
    return Frustum();

  auto physic_to_screen = nodeToScreen();
  if (!physic_to_screen.valid())
    return Frustum();

  return dataset->logicToScreen(physic_to_screen);
}


///////////////////////////////////////////////////////////////////////////
Position QueryNode::getQueryLogicPosition() 
{
  auto dataset = getDataset();
  if (!dataset)
    return Position();

  auto query_bounds = getQueryBounds();
  if (!query_bounds.valid())
    return Position();

  auto physic_to_screen = nodeToScreen();
  if (physic_to_screen.valid())
  {
    auto map = FrustumMap(physic_to_screen);
    query_bounds = Position::shrink(physic_to_screen.getScreenBox(), map, query_bounds);
    if (!query_bounds.valid())
      return Position();
  }

  //find intersection with dataset box
  auto logic_position = dataset->physicToLogic(query_bounds);
  auto map = MatrixMap(Matrix::identity(dataset->getPointDim()));
  logic_position = Position::shrink(dataset->getLogicBox().castTo<BoxNd>(), map, logic_position);

  if (!logic_position.valid())
    return Position();

  return logic_position;
}

///////////////////////////////////////////////////////////////////////////
bool QueryNode::processInput()
{
  abortProcessing();

  auto failed = [&]() {
    publishDumbArray();
    return false;
  };

  auto dataset = getDataset();
  if (!dataset)
    return failed();

  //create (and store in my class the access)
  if (!this->access)
  {
    auto access_configs = dataset->getAccessConfigs();

    if (this->accessindex >= 0 && this->accessindex < (int)access_configs.size())
      setAccess(dataset->createAccess(*access_configs[this->accessindex]));
    else
      setAccess(dataset->createAccess());
  }
 
  addNodeJob(std::make_shared<MyJob>(this, dataset, access));

  return true;
}

//////////////////////////////////////////////////////////////////
void QueryNode::publishDumbArray()
{
  auto buffer=std::make_shared<Array>();
  buffer->bounds=Position::invalid();

  DataflowMessage msg;
  msg.writeValue("data", buffer);
  publish(msg);
}


//////////////////////////////////////////////////////////////////
void QueryNode::exitFromDataflow() 
{
  Node::exitFromDataflow();
  this->access.reset();
}

//////////////////////////////////////////////////////////////////
void QueryNode::writeTo(StringTree& out) const
{
  Node::writeTo(out);

  out.writeValue("verbose", cstring(verbose));
  out.writeValue("accessindex",cstring(accessindex));
  out.writeValue("view_dependent",cstring(bViewDependentEnabled));
  out.writeValue("progression",std::to_string(progression));
  out.writeValue("quality",std::to_string(quality));

  out.writeObject("bounds", node_bounds);

  //position=fn(tree_position)
}

//////////////////////////////////////////////////////////////////
void QueryNode::readFrom(StringTree& in) 
{
  Node::readFrom(in);

  this->verbose = cint(in.readValue("verbose"));
  this->accessindex=cint(in.readValue("accessindex"));
  this->bViewDependentEnabled=cbool(in.readValue("view_dependent"));
  this->progression=(QueryProgression)cint(in.readValue("progression"));
  this->quality=(QueryQuality)cint(in.readValue("quality"));

  in.readObject("bounds", node_bounds);

  //position=fn(tree_position)
}



} //namespace Visus

