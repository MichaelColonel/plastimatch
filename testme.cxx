/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include <iostream>
#include <string>
#include <map>
#include "itkImage.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIterator.h"
#include "itkRGBPixel.h"
enum PixelTypes {
  Char,
  UnsignedChar,
  Short,
  UnsignedShort,
  Int,
  UnsignedInt,
  Float, 
  Double, 
  ThreeVectorComplex,
  Rgb,
  Crap
};

typedef std::complex<float> ThreeVectorComplexPixelType;
typedef itk::RGBPixel<unsigned char> RGBPixelType;

std::map<PixelTypes,char *> typenames;
void typenamesInit()
{
  typenames[Char] = "Char";
  typenames[UnsignedChar] = "UnsignedChar";
  typenames[Short] = "Short";
  typenames[UnsignedShort] = "UnsignedShort";
  typenames[Int] = "Int";
  typenames[UnsignedInt] = "UnsignedInt";
  typenames[Float] = "Float";
  typenames[Double] = "Double";
  typenames[ThreeVectorComplex] = "ThreeVectorComplex";
  typenames[Rgb] = "Rgb";
  typenames[Crap] = "Crap";
}
std::string filename;
int x = -1,y = -1,z = -1, t = -1;
PixelTypes PType(Crap);
double value = 0.0;
int numdims = -1;
bool
ProcessArgs(int argc, char **argv)
{
  while(--argc)
    {
    std::string arg(argv[1]);
    ++argv;
    if(arg == "-2")
      {
      numdims = 2;
      }
    else if(arg == "-3")
      {
      numdims = 3;
      }
    else if(arg == "-4")
      {
      numdims = 4;
      }
    else if(arg == "-f" || arg == "--filename")
      {
      --argc;
      if(argc == 0)
        {
        std::cerr << "missing filename argument" << std::endl;
        return false;
        }
      filename = argv[1];
      ++argv;
      }
    else if(arg == "-v" || arg == "--value")
      {
      if(argc < 1)
        {
        std::cerr << "missing value argument" << std::endl;
        return false;
        }
      --argc;
      ++argv;
      value = atof(*argv);
      }
    else if(arg == "-d" || arg == "--dim")
      {
      if(numdims == -1)
        {
        std::cerr << "missing Number of Dimensions" << std::endl;
        return false;
        }
      if(argc >= numdims)
        {
        x = atoi(argv[1]);
        y = atoi(argv[2]);
        if(numdims > 2)
          {
          z = atoi(argv[3]);
          }
        if(numdims > 3)
          {
          t = atoi(argv[4]);
          }
        argv += numdims;
        argc -= numdims;
        }
      else
        {
        std::cerr << "missing dimensions argument" << std::endl;
        return false;
        }
      }
    else if(arg == "char") { PType = Char; }
    else if(arg == "uchar") { PType = UnsignedChar; }
    else if(arg == "short") { PType = Short; }
    else if(arg == "ushort") { PType = UnsignedShort; }
    else if(arg == "int") { PType = Int; }
    else if(arg == "uint") { PType = UnsignedInt; }
    else if(arg == "float") { PType = Float; }
    else if(arg == "double") { PType = Double; }
    else if(arg == "threevectorcomplex") { PType = ThreeVectorComplex; }
    else if(arg == "rgb") { PType = Rgb; }
    }
  if(PType == Crap)
    {
    std::cerr << "Missing or incorrect pixel type" << std::endl;
    return false;
    }
  if(x <= 0 || y <= 0 || 
     (numdims > 3 && t <= 0) ||
     (numdims > 2 && z <= 0))
    {
    std::cerr << "Missing or incorrect dimensions:" << x << " " << y << " " << z << std::endl;
    return false;
    }
  if(filename == "")
    { 
    std::cerr << "Missing filename parameter" << std::endl;
    }
  return true;
}

template <typename PixelType,unsigned dim>
int MakeImage(const std::string &filename,
              unsigned xdim,
              unsigned ydim, 
              unsigned zdim,
              unsigned tdim,
              PixelType value)
{
  typedef typename itk::Image<PixelType,dim> ImageType;
  typedef typename ImageType::Pointer ImagePointerType;
  typedef typename ImageType::RegionType RegionType;
  typedef typename RegionType::SizeType SizeType;
  typedef typename itk::ImageRegionIterator<ImageType> IteratorType;
  typedef itk::ImageFileWriter<ImageType> ImageFileWriterType;
  typedef typename ImageFileWriterType::Pointer ImageFileWriterPointerType;
  try
    {
    SizeType size;
    size.SetElement(0,xdim);
    size.SetElement(1,ydim);
    if(dim > 2)
      {
      size.SetElement(2,zdim);
      }
    if(dim > 3)
      {
      size.SetElement(3,tdim);
      }
    RegionType region;
    region.SetSize(size);

    ImagePointerType image = ImageType::New();
    image->SetRegions(region);
    image->Allocate();
    IteratorType it(image,region);
    for(it.GoToBegin(); it != it.End(); ++it)
      {
      it.Set(value);
      }
    ImageFileWriterPointerType writer =
      ImageFileWriterType::New();
    writer->SetFileName(filename.c_str());
    writer->SetInput(image);
    writer->Write();
    }
  catch( itk::ExceptionObject &excp )
    {
    std::cerr << "Can't write " << filename << std::endl;
    return 1;
    }
  return 0;
}


int main(int argc,char **argv)
{
  typenamesInit();
  if(!ProcessArgs(argc,argv))
    {
    exit(1);
    }
  std::cerr << "Filename: " << filename << " Dimensions: " << x << " "
            << y << " " << z << " " << " Value: " << value
            << " Type: " << typenames[PType] << std::endl;
#define AllTypesSwitch(dim)                                             \
  switch(PType)                                                         \
    {                                                                   \
    case Char:                                                          \
      exit(MakeImage<char,dim>(filename,x,y,z,t,                        \
                               static_cast<char>(value)));              \
      break;                                                            \
    case UnsignedChar:                                                  \
      exit(MakeImage<unsigned char,dim>(filename,x,y,z,t,               \
                                        static_cast<unsigned char>(value))); \
      break;                                                            \
    case Short:                                                         \
      exit(MakeImage<short,dim>(filename,x,y,z,t,                       \
                                static_cast<short>(value)));            \
      break;                                                            \
    case UnsignedShort:                                                 \
      exit(MakeImage<unsigned short,dim> \
           (filename,x,y,z,t,                                           \
                                         static_cast<unsigned short>(value))); \
      break;                                                            \
    case Int:                                                           \
      exit(MakeImage<int,dim>(filename,x,y,z,t,                         \
                              static_cast<int>(value)));                \
      break;                                                            \
    case UnsignedInt:                                                   \
      exit(MakeImage<unsigned int,dim>\
           (filename,x,y,z,t,                                           \
                                       static_cast<unsigned int>(value))); \
      break;                                                            \
    case Float:                                                         \
      exit(MakeImage<float,dim>(filename,x,y,z,t,                       \
                                static_cast<float>(value)));            \
      break;                                                            \
    case Double:                                                        \
      exit(MakeImage<double,dim>(filename,x,y,z,t,                      \
                                 static_cast<double>(value)));          \
      break;                                                            \
    case ThreeVectorComplex:                                            \
      exit(MakeImage<ThreeVectorComplexPixelType,dim>                   \
           (filename,x,y,z,t,                                           \
            static_cast<ThreeVectorComplexPixelType>(value)));          \
      break;                                                            \
    case Rgb:                                                           \
      exit(MakeImage<RGBPixelType,dim>                                  \
           (filename,x,y,z,t,                                           \
            static_cast<RGBPixelType>(value)));                         \
      break;                                                            \
    default:                                                            \
      exit(1);                                                          \
    }
  switch(numdims)
    {
    case 4:
      AllTypesSwitch(4);
      break;
    case 3:
      AllTypesSwitch(3);
      break;
    case 2:
      AllTypesSwitch(2);
      break;
    }
}
