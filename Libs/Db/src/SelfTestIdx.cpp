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

#include <Visus/Encoder.h>
#include <Visus/IdxDataset.h>

namespace Visus {


VISUS_DB_API void Tutorial_1(String default_layout);
VISUS_DB_API void Tutorial_2(String default_layout);
VISUS_DB_API void Tutorial_3(String default_layout);
VISUS_DB_API void Tutorial_6(String default_layout);

////////////////////////////////////////////////////////////////////////////////////
static BoxNi GetRandomUserBox(int pdim, bool bFullBox)
{
  BoxNi ret(PointNi(pdim), PointNi::one(pdim));

  for (int D = 0; D < pdim; D++)
  {
    switch (pdim)
    {
    case 2:
      if (bFullBox)
      {
        //from 16 to 256
        ret.p2[D] = (((Int64)1) << Utils::getRandInteger(4, 8));
      }
      else
      {
        ret.p1[D] = Utils::getRandInteger(0, 254);
        ret.p2[D] = Utils::getRandInteger(2 + (int)ret.p1[D], 256);
      }
      break;

    case 3:
      if (bFullBox)
      {
        //from 16 to 64
        ret.p2[D] = ((Int64)1) << Utils::getRandInteger(4, 6);
      }
      else
      {
        ret.p1[D] = Utils::getRandInteger(0, 62);
        ret.p2[D] = Utils::getRandInteger(2 + (int)ret.p1[D], 64);
      }
      break;

    case 4:
      if (bFullBox)
      {
        //from 16 to 32
        ret.p2[D] = ((Int64)1) << Utils::getRandInteger(4, 5);
      }
      else
      {
        ret.p1[D] = Utils::getRandInteger(0, 30);
        ret.p2[D] = Utils::getRandInteger(2 + (int)ret.p1[D], 32);
      }
      break;

    case 5:
      if (bFullBox)
      {
        ret.p2[D] = 16;
      }
      else
      {
        ret.p1[D] = Utils::getRandInteger(0, 14);
        ret.p2[D] = Utils::getRandInteger(2 + (int)ret.p1[D], 16);
      }
      break;
    }
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////////
static DType GetRandomDType()
{
  bool byte_aligned = (Utils::getRandInteger(0, 1) ? true : false);
  bool decimal = (Utils::getRandInteger(0, 1) ? true : false);
  bool unsign = !decimal && (Utils::getRandInteger(0, 1) ? true : false);
  int  bitsize = byte_aligned ? (8 * Utils::getRandInteger(1, 4)) : (Utils::getRandInteger(1, 64));
  return DType(unsign, decimal, bitsize);
}



////////////////////////////////////////////////////////////////////////////////////
class SelfTest
{
public:

  //constructor (create random data per slice)
  SelfTest(IdxDataset* dataset)
  {
    this->dataset = dataset;
    this->dtype = dataset->getField().dtype;
    this->user_box = dataset->getLogicBox();
    this->pdim = dataset->getPointDim();
    this->nslices = user_box.p2[pdim - 1] - user_box.p1[pdim - 1];

    //calculate number of samples per slice
    this->perslice = 1;
    Int64 _stride = 1;
    this->stride = PointNi(pdim);
    for (int D = 0; D < (pdim - 1); D++)
    {
      Int64 num = user_box.p2[D] - user_box.p1[D];
      this->perslice *= num;
      stride[D] = _stride;
      _stride *= num;
    }

    PrintInfo("Starting self test procedure on dataset pdim", dataset->getPointDim());

    if (bool bWriteData = true)
    {
      auto access = dataset->createAccess();

      int cont = 0;
      for (int N = 0; N < nslices; N++)
      {
        BoxNi slice_box = getSliceBox(N);

        auto query = dataset->createBoxQuery(slice_box, 'w');
        dataset->beginBoxQuery(query);
        VisusReleaseAssert(query->isRunning());

        Array buffer(query->getNumberOfSamples(), dtype);
        for (int i = 0; i < buffer.c_size(); i++)
          buffer.c_ptr()[i] = cont++;
        query->buffer = buffer;

        VisusReleaseAssert(dataset->executeBoxQuery(access, query));
        this->write_queries.push_back(query);
      }
    }

    if (bool bVerifyData = true)
    {
      auto access = dataset->createAccess();

      for (int N = 0; N < this->nslices; N++)
      {
        auto read_slice = dataset->createBoxQuery(getSliceBox(N), 'r');
        dataset->beginBoxQuery(read_slice);
        VisusReleaseAssert(read_slice->isRunning());
        VisusReleaseAssert(dataset->executeBoxQuery(access, read_slice));
        VisusReleaseAssert(read_slice->getNumberOfSamples().innerProduct() == this->perslice);
        VisusReleaseAssert(CompareSamples(write_queries[N]->buffer, 0, read_slice->buffer, 0, perslice));
        read_slice.reset();
      }
    }
  }

  //execute a random query
  void executeRandomQuery()
  {
    int                  maxh = dataset->getMaxResolution();
    int                  firsth = Utils::getRandInteger(0, maxh);
    int                  lasth = Utils::getRandInteger(firsth, maxh);
    int                  deltah = firsth == lasth ? 1 : Utils::getRandInteger(1, lasth - firsth);
    BoxNi                box = Utils::getRandInteger(0, 1) ? dataset->getLogicBox() : getRandomBox();
    bool                 bInterpolate = Utils::getRandInteger(0, 1) ? true : false;

    static int nactivation = 0;
    nactivation++;

    auto access = dataset->createAccess();

    auto query = dataset->createBoxQuery(box, 'r');
    query->merge_mode = (bInterpolate ? MergeMode::InterpolateSamples : MergeMode::InsertSamples);

    for (int h = firsth; h <= lasth; h = h + deltah)
      query->end_resolutions.push_back(h);

    Array buffer;
    BoxNi h_box;
    PointNi shift(pdim);

    //probably the bounding box cannot get samples
    dataset->beginBoxQuery(query);

    if (!query->isRunning())
      return;

    while (query->isRunning())
    {
      VisusReleaseAssert(dataset->executeBoxQuery(access, query));
      buffer = query->buffer;
      h_box = query->logic_samples.logic_box;
      shift = query->logic_samples.shift;
      dataset->nextBoxQuery(query);
    }

    VisusReleaseAssert(buffer);
    {
      //verify written data
      int nsample = 0;
      for (auto loc = ForEachPoint(buffer.dims); !loc.end(); loc.next())
      {
        PointNi world_point = h_box.p1 + ((loc.pos).leftShift(shift));

        //number of slice
        int N = (int)(world_point[pdim - 1] - user_box.p1[pdim - 1]);

        //position inside the slice buffer
        LogicSamples samples = write_queries[N]->logic_samples;
        PointNi P = samples.logicToPixel(world_point);
        Int64 pos = stride.dotProduct(P);
        VisusReleaseAssert(CompareSamples(this->write_queries[N]->buffer, pos, buffer, nsample, 1));
        nsample++;
      }

      PrintInfo("done query", "first_resolution", firsth, "last_resolution", lasth, "delta_between_resolution", deltah, "merge_mode", bInterpolate ? "interpolate" : "insert");
    }
  }

  //return a random box inside the user_box (to exec read query)
  BoxNi getRandomBox()
  {
    BoxNi ret(PointNi(pdim), PointNi::one(pdim));
    for (int D = 0; D < pdim; D++)
    {
      ret.p1[D] = Utils::getRandInteger((int)user_box.p1[D], (int)user_box.p2[D] - 1);
      ret.p2[D] = Utils::getRandInteger((int)ret.p1[D] + 1, (int)user_box.p2[D]);
    }
    return ret;
  }

  //getSliceBox
  BoxNi getSliceBox(int N) const {
    auto z1 = (int)user_box.p1[pdim - 1] + N;
    auto z2 = z1 + 1;
    return user_box.getSlab(pdim - 1, z1, z2);
  }

protected:

  IdxDataset* dataset;
  DType                 dtype;
  Int64      nslices;
  Int64                 perslice;
  int                   pdim;
  PointNi               stride;
  BoxNi                 user_box;
  std::vector< SharedPtr<BoxQuery> >   write_queries;

}; //end class 


/////////////////////////////////////////////////////
void SelfTestIdx(int max_seconds)
{
  Time t1 = Time::now();

#if 1
  for (auto rowmajor : { false,true })
  {
    String default_layout = rowmajor ? "rowmajor" : "hzorder";

    PrintInfo("Running Tutorial_1...");
    Tutorial_1(default_layout);
    PrintInfo("...done");

    PrintInfo("Running Tutorial_2...");
    Tutorial_2(default_layout);
    PrintInfo("...done");

    PrintInfo("Running Tutorial_3...");
    Tutorial_3(default_layout);
    PrintInfo("...done");

    //remove data from tutorial_1
    try
    {
      auto dataset = LoadIdxDataset("tmp/tutorial_1/visus.idx");
      dataset->removeFiles();
    }
    catch (...) {}

    PrintInfo("Running Tutorial_6...");
    Tutorial_6(default_layout);
    PrintInfo("...done");
  }
#endif

  ////do self testing on random field
  PrintInfo("Running self test procedure max_seconds", max_seconds, "...");

  //test byte aligned query (the fast loop has special sample copy for byte aligned data)
  if (true)
  {
    for (auto rowmajor : { false,true })
    {
      for (int pdim = 2; pdim <= 5; pdim++)
      {
        for (int nbits = 8; nbits <= 64; nbits += 8)
        {
          BoxNi user_box = GetRandomUserBox(pdim, Utils::getRandInteger(0, 1) ? true : false);

          IdxFile idxfile;
          idxfile.logic_box = user_box;
          {
            Field field("myfield", DType(true, false, nbits));
            field.default_layout = rowmajor ? "rowmajor" : "hzorder";
            field.default_compression = Utils::getRandInteger(0, 1) ? "lz4" : "";
            idxfile.fields.push_back(field);
          }

          auto filename = "tmp/self_test_idx/temp.idx";
          idxfile.save(filename);
          auto dataset = LoadIdxDataset(filename);

          {
            SelfTest selftest(dataset.get());
            for (int n = 0; n < 10; n++)
              selftest.executeRandomQuery();
          }

          dataset->removeFiles();
        }
      }
    }
  }

  while (true)
  {
    if (max_seconds > 0 && t1.elapsedSec() > max_seconds)
      break;

    //test 10 queries on a random field
    {
      int      pdim = Utils::getRandInteger(2, 5);
      BoxNi    user_box = GetRandomUserBox(pdim, Utils::getRandInteger(0, 1) ? true : false);

      IdxFile idxfile;
      idxfile.logic_box = user_box;
      {
        Field field("myfield", DType(Utils::getRandInteger(1, 4), GetRandomDType()));
        field.default_layout = Utils::getRandInteger(0, 1) ? "rowmajor" : "hzorder";
        field.default_compression = Utils::getRandInteger(0, 1) ? "lz4" : "";
        idxfile.fields.push_back(field);
      }

      String filename = "tmp/self_test_idx/temp.idx";
      idxfile.save(filename);
      auto dataset = LoadIdxDataset(filename);

      {
        SelfTest selftest(dataset.get());
        for (int n = 0; n < 10; n++)
          selftest.executeRandomQuery();
      }
      dataset->removeFiles();
    }
  }


#if defined(WIN32) && defined(_DEBUG)
  {printf("Selftest OK. Press a char to finish\n"); getchar(); }
#endif
}


} //namespace Visus
