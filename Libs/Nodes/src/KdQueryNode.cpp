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


#include <Visus/KdQueryNode.h>
#include <Visus/GoogleMapsDataset.h>
#include <Visus/NetService.h>
#include <Visus/FieldNode.h>
#include <Visus/StringTree.h>
#include <Visus/IdxDataset.h>
#include <Visus/IdxFilter.h>

namespace Visus {

////////////////////////////////////////////////////////////////
class KdQueryJob : public NodeJob 
{
public:

  KdQueryNode*                  owner = nullptr;

  bool                          verbose = false;

  int                           kdquery_mode =KdQueryMode::NotSpecified;

  Frustum                       logic_to_screen;

  SharedPtr<KdArray>            kdarray;
  SharedPtr<Access>             access;

  SharedPtr<Dataset>            dataset;
  Field                         field;
  double                        time=0;
  Position                      logic_position;

  int                           maxh;
  int                           pdim;
  DatasetBitmask                bitmask;

  Time                          last_publish = Time::now();
  int                           publish_interval=0;
  
  int                           bitsperblock=0;
  bool                          bBlocksAreFullRes=false;

  //constructor
  KdQueryJob() {
  }

  //destructor
  virtual ~KdQueryJob() {
  }

  //publish
  void publish(bool bForce)
  {
    if (aborted() || !owner)
      return;

    if (bForce || last_publish.elapsedMsec() > publish_interval)
    {
      DataflowMessage msg;
      msg.writeValue("kdarray", kdarray);
      owner->publish(msg);
      last_publish = Time::now();
    }
  }

  //readRoot
  bool readRoot(ScopedReadLock& rlock)
  {
    VisusAssert(!kdarray->root);
    VisusAssert(bitsperblock);

    auto pow2_dims= bitmask.getPow2Dims();
    auto pow2_box=bitmask.getPow2Box();

    int max_resolution=this->maxh;
    VisusAssert(bitsperblock<=max_resolution);

    int end_resolution=
      (this->kdquery_mode ==KdQueryMode::UseBlockQuery && !bBlocksAreFullRes)?
        std::min(bitsperblock + 1,max_resolution) : //I'm reading block 0+1 for block mode when the blocks are not fullres
        bitsperblock;

    //I use a box query to get the data
    auto query=dataset->createBoxQuery(pow2_box, field, time,'r', this->aborted);
    query->setResolutionRange(0,end_resolution);
    dataset->beginBoxQuery(query);

    if (!dataset->executeBoxQuery(access,query))
      return false;

    auto fullres     = query->buffer;
    auto displaydata = fullres;

    //If I'm getting the data using access, I need to apply the filters... 
    // otherwise is probably a kdquery=block with a remote url, the server will apply the filter
    if (access)
    {
      if (auto idx = std::dynamic_pointer_cast<IdxDataset>(dataset))
      {
        if (auto filter = idx->createFilter(field))
        {
          for (int H = 0; H <= end_resolution; H++)
          {
            query->setCurrentResolution(H);
            filter->internalComputeFilter(query.get(), /*bInverse*/true);
          }
          displaydata = filter->dropExtraComponentIfExists(fullres);
        }
      }
    }

    {
      ScopedWriteLock wlock(rlock);
      kdarray->root = std::make_shared<KdArrayNode>(1); //root has id equals to 1 (important for split)
      kdarray->root->resolution = end_resolution;
      kdarray->root->logic_box = pow2_box;
      kdarray->root->blockdata   = Array(); //it's not a single block, it is block 0+1!
      kdarray->root->fullres     = fullres;
      kdarray->root->displaydata = displaydata;
    }
    
    publish(/*bForce*/true);
    return true;
  }

  //computeFullRes
  void computeFullRes(KdArrayNode* node, ScopedReadLock& rlock)
  {
    //nothing to do
    if (kdquery_mode == KdQueryMode::UseBoxQuery)
      return;

    if (aborted() || !node)
      return;

    if (!node->fullres)
    {
      if (bBlocksAreFullRes)
      {
        if (node->blockdata)
        {
          {
            ScopedWriteLock wlock(rlock);
            node->fullres     = node->blockdata;
            node->displaydata = node->blockdata;
          }
          publish(/*bForce*/false);
        }
      }
      else
      {
        //node->fullres = node->up->fullres+node->blockdata
        if (!node->up->fullres || !node->blockdata)
          return;

        auto query = dataset->createBoxQuery(node->logic_box, field, time, 'r', this->aborted);
        query->setResolutionRange(0, node->resolution);

        if (aborted())
          return;

        dataset->beginBoxQuery(query);

        if (!query->isRunning())
          return;

        auto splitbit = bitmask[node->resolution - 1 - bitsperblock];
        auto upsamplebit = bitmask[node->resolution];

        bool bLeftChild = node->up->left.get() == node ? true : false;

        auto fullres = bLeftChild ?
          ArrayUtils::splitAndGetFirst(node->up->fullres, splitbit, aborted) :
          ArrayUtils::splitAndGetSecond(node->up->fullres, splitbit, aborted);

        if (aborted() || !fullres)
          return;

        fullres = ArrayUtils::upSample(fullres, upsamplebit, aborted);
        if (aborted() || !fullres)
          return;

        VisusAssert(query->getNumberOfSamples() == fullres.dims);

        //prepare to merge samples from current resolution
        query->setCurrentResolution(node->resolution - 1);

        VisusAssert(fullres.dims == query->getNumberOfSamples());
        query->buffer = fullres;

        auto blockquery = dataset->createBlockQuery(getBlockId(node), field, time, 'r', Aborted());
        VisusAssert(blockquery->getNumberOfSamples() == node->blockdata.dims);
        blockquery->buffer = node->blockdata;

        if (aborted() || !dataset->mergeBoxQueryWithBlockQuery(query, blockquery))
          return;

        fullres = query->buffer;

        //this is the latest resolution! needed also for filter->applyToQuery!
        query->setCurrentResolution(node->resolution);

        if (aborted())
          return;

        Array displaydata= fullres;

        //need to apply the filter, from now on I can display the data
        if (auto idx = std::dynamic_pointer_cast<IdxDataset>(dataset))
        {
          if (auto filter = idx->createFilter(field))
          {
            filter->internalComputeFilter(query.get(), /*bInverse*/true);
            displaydata = filter->dropExtraComponentIfExists(fullres);
          }
        }

        //store the results
        {
          ScopedWriteLock wlock(rlock);
          node->blockdata = Array(); //don't need this anymore 
          node->fullres = fullres;
          node->displaydata = displaydata;
        }

        publish(/*bForce*/false);
      }
    }

    computeFullRes(node->left .get(), rlock);
    computeFullRes(node->right.get(), rlock);
  }

  //getBlockId
  BigInt getBlockId(KdArrayNode* node) 
  {
    return BigInt(bBlocksAreFullRes ? node->id - 1 : node->id); //IF !bBlocksAreFullRes node->id is the block number (considering that root has blocks 0 and 1)
  }

  //isLeafNode
  bool isLeafNode(KdArrayNode* node)
  {
    //reached the end resolution
    if (node->resolution >= maxh)
      return true;

    //I can get more resolution up to maxh
    if (!logic_to_screen.valid())
      return false;

    //project the box to the screen and check if the resolution is enough
    FrustumMap map(logic_to_screen);
    auto logic_box = node->logic_box.castTo<BoxNd>();

    //project on the screen
    std::vector<Point2d> screen_points;
    for (auto p : logic_box.getPoints())
    {
      auto logic_point = p.toPoint3();
      screen_points.push_back(map.projectPoint(logic_point));
    }

    double max_screen_distance[3] = { 0,0,0 };
    for (auto edge : BoxNi::getEdges(pdim))
    {
      auto axis = edge.axis;
      auto s0 = screen_points[edge.index0];
      auto s1 = screen_points[edge.index1];
      max_screen_distance[axis] = std::max(max_screen_distance[axis], s0.distance(s1));
    }

    //TODO: can I do better then this to compute number of samples?
    PointNi nsamples;
    {
      auto query = dataset->createBoxQuery(node->logic_box, field, time, 'r');
      query->setResolutionRange(0, node->resolution);
      dataset->beginBoxQuery(query);
      nsamples = query->getNumberOfSamples();
      nsamples.setPointDim(3, 1);
    }

    //samples_per_pixel
    double samples_per_pixel[3] =
    {
      nsamples[0] / max_screen_distance[0],
      nsamples[1] / max_screen_distance[1],
      nsamples[2] / max_screen_distance[2]
    };

    //note: for 3d the last sorted factor will be Z in screen space, which i simply ignore
    std::sort(samples_per_pixel, samples_per_pixel + 3);
    auto quality = sqrt(samples_per_pixel[0] * samples_per_pixel[1]);
    return quality >= 1.0 ? true : false;
  } 

  //runJobUsingBlockQuery
  void runJobUsingBlockQuery()
  {
    ScopedReadLock rlock(kdarray->lock);

    if (!kdarray->root && !readRoot(rlock))
      return;

    WaitAsync< Future<Void> > wait_async;
    VisusAssert(access);
    access->beginRead();

    std::deque<KdArrayNode*> bfs;
    bfs.push_back(kdarray->root.get());
    while (!bfs.empty() && !aborted())
    {
      KdArrayNode* node = bfs.front(); bfs.pop_front();
      VisusAssert(node);

      //not visible, just drop it
      if (!kdarray->isNodeVisible(node))
      {
        ScopedWriteLock wlock(rlock);
        kdarray->clearChilds(node);
        continue;
      }

      //recursive splitting
      if (!isLeafNode(node))
      {
        //make sure it's split
        if (node->isLeaf())
        {
          ScopedWriteLock wlock(rlock);
          auto splitbit = bitmask[1 + node->resolution - kdarray->root->resolution];
          kdarray->split(node, splitbit); //jump the 'V'
        }

        bfs.push_back(node->left.get());
        bfs.push_back(node->right.get());
      }
      else
      {
        //drop unneeded levels
        if (!node->isLeaf())
        {
          ScopedWriteLock wlock(rlock);
          kdarray->clearChilds(node);
        }
      }

      //could be that I skipped the computation of fullres for aborted() signal
      computeFullRes(node, rlock);

      //all done for the current node
      if (node->fullres)
        continue;

      //already got the blockdata
      if (node->blockdata)
        continue;

      //for bBlocksAreFullRes I execute only final levels (since I don't need any merging)
      if (bBlocksAreFullRes && !isLeafNode(node))
        continue;

      //retrieve the block data
      auto blockquery = dataset->createBlockQuery(getBlockId(node), field, time, 'r', this->aborted);
      dataset->executeBlockQuery(access, blockquery);
      wait_async.pushRunning(blockquery->done).when_ready([this, blockquery, node, &rlock](Void) {

        if (aborted() || blockquery->failed())
          return;

        VisusAssert(!node->fullres && !node->blockdata);

        //make sure is row major
        if (!blockquery->buffer.layout.empty())
          dataset->convertBlockQueryToRowMajor(blockquery);

        if (!blockquery->buffer)
          return;

        //need the write lock here
        {
          ScopedWriteLock wlock(rlock);
          node->blockdata = blockquery->buffer;
        }

        computeFullRes(node, rlock);
      });

    };

    access->endRead();

    wait_async.waitAllDone();

    if (aborted())
      return;

    publish(/*bForce*/true);

    if (verbose)
      this->access->printStatistics();
  }

  //runJobUsingQuery
  void runJobUsingQuery()
  {
    ScopedReadLock rlock(kdarray->lock);

    if (!kdarray->root && !readRoot(rlock))
      return;

    //execute remote queries in async way
    SharedPtr<NetService> netservice;
    if (!access && Url(dataset->getUrl()).isRemote())
      netservice = std::make_shared<NetService>(8);

    WaitAsync< Future<NetResponse> >  wait_async;

    std::deque<KdArrayNode*> bfs;
    bfs.push_back(kdarray->root.get());
    while (!bfs.empty() && !aborted())
    {
      KdArrayNode* node = bfs.front(); bfs.pop_front();
      VisusAssert(node);

      //not visible, just drop it
      if (!kdarray->isNodeVisible(node))
      {
        ScopedWriteLock wlock(rlock);
        kdarray->clearChilds(node);
        continue;
      }

      //recursive splitting
      if (!isLeafNode(node))
      {
        //make sure it's split
        if (node->isLeaf())
        {
          ScopedWriteLock wlock(rlock);
          kdarray->split(node, bitmask[1 + node->resolution - kdarray->root->resolution]); //jump the 'V'
        }

        bfs.push_back(node->left.get());
        bfs.push_back(node->right.get());
      }
      else
      {
        //drop unneeded levels
        if (!node->isLeaf())
        {
          ScopedWriteLock wlock(rlock);
          kdarray->clearChilds(node);
        }
      }

      //could be that I skipped the computation of fullres for aborted() signal
      computeFullRes(node, rlock);

      //all done for the current node
      if (node->fullres)
        continue;

      //for Query I execute only final level
      if (!isLeafNode(node))
        continue;

      auto query = dataset->createBoxQuery(node->logic_box, field, time,'r', this->aborted);
      query->setResolutionRange(0, node->resolution);

      dataset->beginBoxQuery(query);

      if (!query->isRunning() || !query->allocateBufferIfNeeded())
        continue;

      //remote 'Query'
      if (netservice)
      {
        auto request = std::make_shared<NetRequest>(dataset->createBoxQueryRequest(query));

        wait_async.pushRunning(NetService::push(netservice, *request)).when_ready([this, query, node, &rlock](NetResponse response) {

          VisusAssert(kdquery_mode == KdQueryMode::UseBoxQuery);

          if (this->aborted() || !response.isSuccessful())
            return;

          auto decoded = response.getCompatibleArrayBody(query->getNumberOfSamples(), query->field.dtype);
          if (!decoded)
            return;

          //need the write lock here
          {
            ScopedWriteLock wlock(rlock);
            VisusAssert(!node->fullres);
            node->fullres     = decoded;
            node->displaydata = decoded;
          }

          this->publish(false);
        });
      }

      //execute in place (TODO: use thread here?)
      else if (dataset->executeBoxQuery(access, query))
      {
        {
          ScopedWriteLock wlock(rlock);
          node->fullres = query->buffer;
          node->displaydata = query->buffer;
        }

        publish(false);
      }
    }

    wait_async.waitAllDone();

    if (aborted())
      return;

    publish(/*bForce*/true);

    if (verbose)
      this->access->printStatistics();
  }

  //runJob
  virtual void runJob() override
  {
    if (kdquery_mode == KdQueryMode::UseBlockQuery)
      runJobUsingBlockQuery();
    else
      runJobUsingQuery();
  }

};


///////////////////////////////////////////////////////////////////////////////////////////////
KdQueryNode::KdQueryNode() {
  removeOutputPort("array");
  addOutputPort("kdarray");
}

///////////////////////////////////////////////////////////////////////////////////////////////
KdQueryNode::~KdQueryNode() {
}



///////////////////////////////////////////////////////////////////////////////////////////////
bool KdQueryNode::processInput()
{
  abortProcessing();
   
  auto dataset         = readValue<Dataset>("dataset");
  auto fieldname       = readString("fieldname");
  auto time            = readDouble("time");

  int kdquery_mode=dataset? dataset->getKdQueryMode() : KdQueryMode::NotSpecified;
  if (kdquery_mode==KdQueryMode::NotSpecified) 
    kdquery_mode=KdQueryMode::UseBlockQuery;

  VisusAssert(kdquery_mode==KdQueryMode::UseBoxQuery || kdquery_mode==KdQueryMode::UseBlockQuery);

  //I cannot recycle the current kdarray
  if (!kdarray || !(dataset && this->dataset==dataset && this->time==time && this->fieldname==fieldname))
  {
    kdarray=std::make_shared<KdArray>(dataset? dataset->getPointDim() : 0);
    this->dataset      = dataset;
    this->time         = time;
    this->fieldname    = fieldname;
    this->setAccess(SharedPtr<Access>());
    if (dataset)
    {
      auto access_configs=dataset->getAccessConfigs();
      int accessindex=getAccessIndex();
      if (accessindex>=0 && accessindex<(int)access_configs.size())
        this->setAccess(dataset->createAccess(*access_configs[accessindex]));
      else
        this->setAccess(kdquery_mode==KdQueryMode::UseBlockQuery? dataset->createAccessForBlockQuery() : dataset->createAccess());
    }
  }

  if (!kdarray)
    return false;

  if (!dataset)
    return false;

  if (!dataset->getTimesteps().containsTimestep(time))
    return false;

  auto field = fieldname.empty() ? dataset->getField() : dataset->getField(fieldname);
  if (!field.valid())
    return false;

  auto logic_position = getQueryLogicPosition();
  if (!logic_position.valid())
    return false;

  //i need an access for accessing block
  if (kdquery_mode == KdQueryMode::UseBlockQuery && !access)
    return false;


  auto job=std::make_shared<KdQueryJob>();
  job->owner = this;
  job->kdquery_mode = kdquery_mode;
  job->publish_interval = dataset->getPointDim() == 3 ? 2000 : 200;
  job->dataset=dataset;
  job->access=access;
  job->kdarray=kdarray;
  job->time=time;
  job->field=field;
  job->logic_position = logic_position;
  job->bitsperblock = access ? access->bitsperblock : dataset->getDefaultBitsPerBlock();
  job->logic_to_screen = this->logicToScreen();
  job->maxh = dataset->getMaxResolution();
  job->pdim = dataset->getPointDim();
  job->bitmask= dataset->getBitmask();

  //need write lock here
  {
    ScopedWriteLock wlock(kdarray->lock);

    //remove transformation, I will add the physic clipping below
    auto logic_box = logic_position.toAxisAlignedBox().castTo<BoxNi>();
    kdarray->logic_box = logic_box;

    //physic coordinates
    kdarray->clipping = dataset->logicToPhysic(logic_position);
    kdarray->bounds   = dataset->logicToPhysic(logic_box);

    job->bBlocksAreFullRes = std::dynamic_pointer_cast<GoogleMapsDataset>(dataset) ? true : false;

    //TODO enable also for UseBlockQuery?
    if (kdquery_mode == KdQueryMode::UseBoxQuery)
    {
      //all levels from [0,cutoff) will be cached without any memory limitations
      //there will be 2^cutoff-1 kdnodes  (8->255 , 9->511, 10->1024)
      const int caching_cutoff = 10;
      kdarray->enableCaching(caching_cutoff, StringUtils::getByteSizeFromString("20mb"));
    }
  }

  addNodeJob(job);
  return true;
} 


} //namespace Visus



