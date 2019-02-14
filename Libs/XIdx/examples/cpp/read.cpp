/*
 * Copyright (c) 2017 University of Utah
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>

#include "Visus/VisusXIdx.h"

using namespace Visus;

int main(int argc, char** argv){
// TODO use cout and toString instead of printf

  if(argc < 2){
    fprintf(stderr, "Usage: read file_path [debug]\n");
    return 1;
  }

  clock_t start, finish;
  start = clock();

  SharedPtr<XIdxFile> metadata = XIdxFile::load(std::string(argv[1]));

  SharedPtr<Group> time_group = metadata->getGroupPtr(GroupType::TEMPORAL_GROUP_TYPE);

  finish = clock();

  printf("Time taken %fms\n",(double(finish)-double(start))/CLOCKS_PER_SEC);

  //SharedPtr<Domain> time_domain = std::static_pointer_cast<HyperSlabDomain>(root_group->domain);
  
  SharedPtr<TemporalListDomain> time_domain = std::static_pointer_cast<TemporalListDomain>(time_group->domain);
  
  printf("Time Domain[%s]:\n", time_domain->type.toString().c_str());

  for(auto& att: time_domain->attributes)
    printf("\t\tAttribute %s value %s\n", att->name.c_str(), att->value.c_str());
  
  int t_count=0;
  for(auto t : time_domain->getLinearizedIndexSpace()){
    printf("Timestep %f\n", t);

    auto grid = time_group->getGroup(t_count++);
    SharedPtr<Domain> spatial_domain = grid->domain;
    
    printf("\tGrid Domain[%s]:\n", spatial_domain->type.toString().c_str());
    
    for(auto& att: spatial_domain->attributes)
      printf("\t\tAttribute %s value %s\n", att->name.c_str(), att->value.c_str());
    
    if(spatial_domain->type == DomainType::SPATIAL_DOMAIN_TYPE){
      SharedPtr<SpatialDomain> sdom = std::dynamic_pointer_cast<SpatialDomain>(spatial_domain);
      printf("\tTopology %s volume %lu\n", sdom->topology->type.toString().c_str(), sdom->getVolume());
      printf("\tGeometry %s", sdom->geometry->type.toString().c_str());
    }
    else if(spatial_domain->type == DomainType::MULTIAXIS_DOMAIN_TYPE)
    {
      SharedPtr<MultiAxisDomain> mdom = std::dynamic_pointer_cast<MultiAxisDomain>(spatial_domain);
      for(auto& axis : mdom->axis){
        printf("\tAxis %s volume %lu: [ ", axis->name.c_str(), axis->getVolume());
        
        // print axis values
        for(auto v: axis->getValues())
          printf("%f ", v);
        printf("]\n");
        
        for(auto& att: axis->attributes)
          printf("\t\tAttribute %s value %s\n", att->name.c_str(), att->value.c_str());
      }
    }
    
    printf("\n");
    
    for(auto& var: grid->variables){
      auto source = var->data_items[0]->findDataSource();
      printf("\t\tVariable: %s ", var->name.c_str());
      if(source != nullptr)
        printf("data source url: %s\n", source->url.c_str());
      else printf("\n");
      
      for(auto att: var->attributes){
        printf("\t\t\tAttribute %s value %s\n", att->name.c_str(), att->value.c_str());
      }
    }
    
  }
  
  // (Debug) Saving the content in a different file to compare with the original
  metadata->save("verify.xidx");
  printf("output saved into verify.xidx\n");

  return 0;
}
