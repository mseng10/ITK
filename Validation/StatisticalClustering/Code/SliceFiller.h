/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef __SliceFiller_h
#define __SliceFiller_h

#include "itkImageToImageFilter.h"

template< class TImage >
class SliceFiller : public itk::ImageToImageFilter< TImage, TImage >
{
public:
  /** Standard class typedefs. */
  typedef SliceFiller  Self;
  typedef itk::ImageToImageFilter< TImage, TImage >  Superclass;
  typedef itk::SmartPointer<Self>  Pointer;
  typedef itk::SmartPointer<const Self>  ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(SliceFiller, ImageToImageFilter);

  typedef typename TImage::SizeType ImageSizeType ;
  typedef typename TImage::PixelType ImagePixelType ;

  void SetStartingSliceNumber(int sliceNumber) ;

  void SetDesiredSize(ImageSizeType size) ;

  void SetBackgroundPixelValue(ImagePixelType value) ;

protected:
  SliceFiller() ;
  ~SliceFiller() ;

  void GenerateData() ;

private:
  typename TImage::PixelType m_BackgroundPixelValue ;
  typename TImage::SizeType m_DesiredSize ;
  int m_SliceSize ;
  int m_StartingSliceNumber ;
} ; // end of class

#ifndef ITK_MANUAL_INSTANTIATION
#include "SliceFiller.txx"
#endif

#endif
