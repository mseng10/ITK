/*=========================================================================

  Program:   Insight Segmentation & Registration Toolkit
  Module:    itkDenseFiniteDifferenceImageFilter.txx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2001 Insight Consortium
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * The name of the Insight Consortium, nor the names of any consortium members,
   nor of any contributors, may be used to endorse or promote products derived
   from this software without specific prior written permission.

  * Modified source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#ifndef __itkDenseFiniteDifferenceImageFilter_txx_
#define __itkDenseFiniteDifferenceImageFilter_txx_

#include <list>
#include "itkImageRegionIterator.h"
#include "itkNumericTraits.h"
#include "itkFastMutexLock.h"

namespace itk {

/** Used for mutex locking */
static SimpleFastMutexLock ListMutex;

template <class TInputImage, class TOutputImage>
void
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::AllocateUpdateBuffer()
{
  // The update buffer looks just like the output.
  typename TOutputImage::Pointer output = this->GetOutput();

  m_UpdateBuffer->SetLargestPossibleRegion(output->GetLargestPossibleRegion());
  m_UpdateBuffer->SetRequestedRegion(output->GetRequestedRegion());
  m_UpdateBuffer->SetBufferedRegion(output->GetBufferedRegion());
  m_UpdateBuffer->Allocate();
}

template<class TInputImage, class TOutputImage>
void
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::ApplyUpdate(TimeStepType dt)
{
  // Set up for multithreaded processing.
  DenseFDThreadStruct str;
  str.Filter = this;
  str.TimeStep = dt;
  this->GetMultiThreader()->SetNumberOfThreads(this->GetNumberOfThreads());
  this->GetMultiThreader()->SetSingleMethod(this->ApplyUpdateThreaderCallback,
                                            &str);
  // Multithread the execution
  this->GetMultiThreader()->SingleMethodExecute();
}

template<class TInputImage, class TOutputImage>
ITK_THREAD_RETURN_TYPE
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::ApplyUpdateThreaderCallback( void * arg )
{
  DenseFDThreadStruct * str;
  int total, threadId, threadCount;

  threadId = ((MultiThreader::ThreadInfoStruct *)(arg))->ThreadID;
  threadCount = ((MultiThreader::ThreadInfoStruct *)(arg))->NumberOfThreads;

  str = (DenseFDThreadStruct *)(((MultiThreader::ThreadInfoStruct *)(arg))->UserData);

  // Execute the actual method with appropriate output region
  // first find out how many pieces extent can be split into.
  // Using the SplitRequestedRegion method from itk::ImageSource.
  ThreadRegionType splitRegion;
  total = str->Filter->SplitRequestedRegion(threadId, threadCount,
                                            splitRegion);
  
  if (threadId < total)
    {
    str->Filter->ThreadedApplyUpdate(str->TimeStep, splitRegion, threadId);
    }

  return ITK_THREAD_RETURN_VALUE;
}

template <class TInputImage, class TOutputImage>
typename
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>::TimeStepType
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::CalculateChange()
{
  int threadCount;
  TimeStepType dt;
  
  // Set up for multithreaded processing.
  DenseFDThreadStruct str;
  str.Filter = this;
  str.TimeStep = NumericTraits<TimeStepType>::Zero;  // Not used during the
                                                  // calculate change step.
  this->GetMultiThreader()->SetNumberOfThreads(this->GetNumberOfThreads());
  this->GetMultiThreader()->SetSingleMethod(this->CalculateChangeThreaderCallback,
                                            &str);

  // Initialize the list of time step values that will be generated by the
  // various threads.  There is one distinct slot for each possible thread,
  // so this data structure is thread-safe.
  threadCount = this->GetMultiThreader()->GetNumberOfThreads();  
  str.TimeStepList = new TimeStepType[threadCount];                 
  str.ValidTimeStepList = new bool[threadCount];
  for (int i =0; i < threadCount; ++i)
    {      str.ValidTimeStepList[i] = false;    } 

  // Multithread the execution
  this->GetMultiThreader()->SingleMethodExecute();

  // Resolve the single value time step to return
  dt = this->ResolveTimeStep(str.TimeStepList, str.ValidTimeStepList, threadCount);
  delete [] str.TimeStepList;
  delete [] str.ValidTimeStepList;

  return  dt;
}

template <class TInputImage, class TOutputImage>
ITK_THREAD_RETURN_TYPE
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::CalculateChangeThreaderCallback( void * arg )
{
  DenseFDThreadStruct * str;
  int total, threadId, threadCount;

  threadId = ((MultiThreader::ThreadInfoStruct *)(arg))->ThreadID;
  threadCount = ((MultiThreader::ThreadInfoStruct *)(arg))->NumberOfThreads;

  str = (DenseFDThreadStruct *)(((MultiThreader::ThreadInfoStruct *)(arg))->UserData);

  // Execute the actual method with appropriate output region
  // first find out how many pieces extent can be split into.
  // Using the SplitRequestedRegion method from itk::ImageSource.
  ThreadRegionType splitRegion;

  total = str->Filter->SplitRequestedRegion(threadId, threadCount,
                                            splitRegion);

  if (threadId < total)
    { 
      str->TimeStepList[threadId]
        = str->Filter->ThreadedCalculateChange(splitRegion, threadId);
      str->ValidTimeStepList[threadId] = true;
    }

  return ITK_THREAD_RETURN_VALUE;  
}

template <class TInputImage, class TOutputImage>
void
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::ThreadedApplyUpdate(TimeStepType dt, const ThreadRegionType &regionToProcess,
                           int threadId)
{
  ImageRegionIterator<UpdateBufferType> u(m_UpdateBuffer,    regionToProcess);
  ImageRegionIterator<OutputImageType>  o(this->GetOutput(), regionToProcess);

  u = u.Begin();
  o = o.Begin();

  while ( !u.IsAtEnd() )
    {
    o.Value() += u.Value() * dt;  // no adaptor support here
    ++o;
    ++u;
    }
}

template <class TInputImage, class TOutputImage>
typename
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>::TimeStepType
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::ThreadedCalculateChange(const ThreadRegionType &regionToProcess, int
                          threadId)
{
  typedef typename OutputImageType::RegionType RegionType;
  typedef typename OutputImageType::SizeType   SizeType;
  typedef typename OutputImageType::SizeValueType   SizeValueType;
  typedef typename OutputImageType::IndexType  IndexType;
  typedef typename OutputImageType::IndexValueType  IndexValueType;
  typedef typename FiniteDifferenceEquationType::BoundaryNeighborhoodType
    BoundaryIteratorType;
  typedef typename FiniteDifferenceEquationType::NeighborhoodType
    NeighborhoodIteratorType;
  typedef ImageRegionIterator<UpdateBufferType> UpdateIteratorType;

  typename OutputImageType::Pointer output = this->GetOutput();
  unsigned int i, j;
  TimeStepType timeStep;
  void *globalData;

  // First we analyze the regionToProcess to determine if any of its faces are
  // along a buffer boundary (we have no data in the buffer for pixels
  // that are outside the boundary and within the neighborhood radius so will
  // have to treat them differently).  We also determine the size of the non-
  // boundary region that will be processed.
  const typename FiniteDifferenceEquationType::Pointer df
    = this->GetDifferenceEquation();
  const IndexType bStart = output->GetBufferedRegion().GetIndex();
  const SizeType  bSize  = output->GetBufferedRegion().GetSize();
  const IndexType rStart = regionToProcess.GetIndex();
  const SizeType  rSize  = regionToProcess.GetSize();
  const SizeType  radius = df->GetRadius();

  IndexValueType  overlapLow, overlapHigh;
  std::vector<RegionType> faceList;
  IndexType  fStart;         // Boundary, "face"
  SizeType   fSize;          // region data.
  RegionType fRegion;
  SizeType   nbSize  = regionToProcess.GetSize();   // Non-boundary region
  IndexType  nbStart = regionToProcess.GetIndex();  // data.
  RegionType nbRegion;

  for (i = 0; i < ImageDimension; ++i)
    {
      overlapLow = (rStart[i] - static_cast<IndexValueType>(radius[i])) 
        - bStart[i];
      overlapHigh= (bStart[i] + static_cast<IndexValueType>(bSize[i])) 
        - (rStart[i] + static_cast<IndexValueType>(rSize[i] + radius[i]));

      if (overlapLow < 0) // out of bounds condition, define a region of 
        {                 // iteration along this face

          // NOTE: this algorithm results in duplicate
          // processing of a single pixel at corners between
          // adjacent faces.  This is negligible performance-
          // wise until very high dimensions.          
          for (j = 0; j < ImageDimension; ++j) 
            { 
              // define the starting index                                 
              // and size of the face region
              fStart[j] = rStart[j];
              // casting from signed to unsigned is ok here since
              // -overLapLow must be positive.
              if ( j == i ) fSize[j] = static_cast<SizeValueType>(-overlapLow);
              else          fSize[j] = rSize[j];   
            }                                      
          nbSize[i]  -= fSize[i];                  
          nbStart[i] += -overlapLow;               
          fRegion.SetIndex(fStart);                
          fRegion.SetSize(fSize);                  
          ListMutex.Lock();
          faceList.push_back(fRegion);             
          ListMutex.Unlock();
        }
      if (overlapHigh < 0)
        {
          for (j = 0; j < ImageDimension; ++j)
            {
              if ( j == i )
                {
                  fStart[j] = rStart[j] + 
                     static_cast<IndexValueType>(rSize[j]) + overlapHigh;
                  // casting from signed to unsigned is ok here since
                  // -overlapHigh must be positive
                  fSize[j] = static_cast<SizeValueType>(-overlapHigh);
                }
              else
                {
                  fStart[j] = rStart[j];
                  fSize[j] = rSize[j];
                }
            }
          nbSize[i] -= fSize[i];
          fRegion.SetIndex(fStart);
          fRegion.SetSize(fSize);
          ListMutex.Lock();
          faceList.push_back(fRegion);
          ListMutex.Unlock();
        }
    }
  nbRegion.SetSize(nbSize);
  nbRegion.SetIndex(nbStart);

  // Initialize the time step.
  //  timeStep = df->GetInitialTimeStep();

  // Ask the function object for a pointer to a data structure it
  // will use to manage any global values it needs.  We'll pass this
  // back to the function object at each calculation and then
  // again so that the function object can use it to determine a
  // time step for this iteration.
  globalData = df->GetGlobalDataPointer();

  // Process the non-boundary region.
  NeighborhoodIteratorType nD(radius, output, nbRegion);
  UpdateIteratorType       nU(m_UpdateBuffer,  nbRegion);
  nD.GoToBegin();
  while( !nD.IsAtEnd() )
    {
      nU.Value() = df->ComputeUpdate(nD, globalData);
      ++nD;
      ++nU;
    }

  // Process each of the boundary faces.
  BoundaryIteratorType bD;
  UpdateIteratorType   bU;
  for (std::vector<RegionType>::iterator fIt = faceList.begin();
       fIt != faceList.end(); ++fIt)
    {
      bD = BoundaryIteratorType(radius, output, *fIt);
      bU = UpdateIteratorType  (m_UpdateBuffer, *fIt);
     
      bD.GoToBegin();
      bU.GoToBegin();
      while ( !bD.IsAtEnd() )
        {
          bU.Value() = df->ComputeUpdate(bD, globalData);
          ++bD;
          ++bU;
        }
    }

  // Ask the finite difference function to compute the time step for
  // this iteration.  We give it the global data pointer to use, then
  // ask it to free the global data memory.
  timeStep = df->ComputeGlobalTimeStep(globalData);
  df->ReleaseGlobalDataPointer(globalData);
  
  return timeStep;
}

template <class TInputImage, class TOutputImage>
void
DenseFiniteDifferenceImageFilter<TInputImage, TOutputImage>
::PrintSelf(std::ostream& os, Indent indent) const
{
  os << indent << "DenseFiniteDifferenceImageFilter";
  Superclass::PrintSelf(os, indent.GetNextIndent());
}

}// end namespace itk

#endif
