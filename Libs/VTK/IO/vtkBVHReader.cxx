/*=========================================================================

  Program: Bender

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

#include "vtkBVHReader.h"

#include "vtkAbstractTransform.h"
#include "vtkArmatureWidget.h"
#include "vtkBoneWidget.h"
#include "vtkCollection.h"
#include "vtkExecutive.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkNew.h"
#include "vtkObjectFactory.h"
#include "vtkPolyData.h"
#include "vtkTransform.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
//----------------------------------------------------------------------------
bool IsBlank(char c)
{
  return c == ' ' || c == '\t';
}

//----------------------------------------------------------------------------
const std::string GetKeyword(const std::string& line)
{
  std::string keyword = "";
  bool wordHasStarted = false;
  for (std::string::const_iterator it = line.begin(); it < line.end(); ++it)
    {
    // stop at the first space after the keyword
    if (keyword != "" && IsBlank(*it))
      {
      return keyword;
      }

    if (*it == '{')
      {
      return "{";
      }
    else if(*it == '}')
      {
      return "}";
      }
    else if(*it == '#')
      {
      return wordHasStarted ? keyword : "#";
      }

    if (!wordHasStarted)
      {
      wordHasStarted = isupper(*it);
      }

    if (wordHasStarted)
      {
      keyword += *it;
      }
    }
  return keyword;
}

//----------------------------------------------------------------------------
const std::string
MoveToNextKeyword(std::ifstream& file, std::string& line)
{
  std::string keyword = GetKeyword(line);
  while(keyword == "#")
    {
    if (!std::getline(file, line))
      {
      return "";
      }
    keyword = GetKeyword(line);
    }

  return keyword;
}

//----------------------------------------------------------------------------
template<typename T>
void GetValues(const std::string& line, std::vector<T>& values)
{
  std::stringstream ss;
  size_t valueStart = 0;
  while (valueStart != std::string::npos)
    {
    size_t valueStop = line.find(" ", valueStart+1);
    ss << line.substr(valueStart, valueStop - valueStart);

    T value;
    ss >> value;
    values.push_back(value);

    ss.clear();
    valueStart = valueStop;
    }
}

//----------------------------------------------------------------------------
template<typename T>
void GetValues(const std::string& line,
               std::vector<T>& values,
               const std::string keyword)
{
  size_t valueStart = line.rfind(keyword) + keyword.size() + 1;
  GetValues<T>(line.substr(valueStart), values);
}

//----------------------------------------------------------------------------
void GetOffset(const std::string& line, double* offset)
{
  std::vector<double> offsetVect;
  GetValues<double>(line, offsetVect, "OFFSET");
  std::copy(offsetVect.begin(), offsetVect.end(), offset);
}

//----------------------------------------------------------------------------
void GetChannels(const std::string& line, std::vector<std::string>& channels)
{
  GetValues<std::string>(line, channels, "CHANNELS");
}

//----------------------------------------------------------------------------
template<typename T> T GetValue(const std::string& line, std::string keyword)
{
  std::vector<T> values;
  GetValues<T>(line, values, keyword);
  assert(values.size() == 1);
  return values[0];
}

//----------------------------------------------------------------------------
std::string
GetBoneName(const std::string& line, std::string keyword = "JOINT")
{
  return GetValue<std::string>(line, keyword);
}

//----------------------------------------------------------------------------
vtkQuaterniond GetParentToBoneRotation(
  std::vector<double>::const_iterator& valueIterator,
  std::vector<std::string> channel,
  vtkBoneWidget* bone,
  vtkQuaterniond& initialRotation)
{
  // /!\ Ignore any translation
  // \todo (?) Stop ignoring translations

  vtkQuaterniond rotation;
  for (std::vector<std::string>::iterator it = channel.begin();
    it != channel.end(); ++it)
    {
    if (*it == "Xrotation" || *it == "Yrotation" || *it == "Zrotation")
      {
      double axis[3];
      axis[0] = (*it == "Xrotation") ? 1.0 : 0.0;
      axis[1] = (*it == "Yrotation") ? 1.0 : 0.0;
      axis[2] = (*it == "Zrotation") ? 1.0 : 0.0;

      vtkQuaterniond newRotation;
      newRotation.SetRotationAngleAndAxis(
        vtkMath::RadiansFromDegrees(*valueIterator), axis);
      newRotation.Normalize();

      rotation = rotation * newRotation;
      rotation.Normalize();

      ++valueIterator;
      }
   else if (*it == "Xposition" || *it == "Yposition" || *it == "Zposition")
      {
      ++valueIterator;
      }
    }

  // First put the rotation in the initial transform world
  rotation = initialRotation * rotation * initialRotation.Inverse();

  // then in the world's coordinates
  vtkQuaterniond worldToParentRest = bone->GetWorldToParentRestRotation();
  vtkQuaterniond parentToWorldRest = worldToParentRest.Inverse();
  return (parentToWorldRest * rotation * worldToParentRest).Normalized();
}

//----------------------------------------------------------------------------
void
GetParentToBoneRotations(std::vector<double>& values,
                         std::vector< std::vector<std::string> >& channels,
                         std::vector<vtkQuaterniond>& rotations,
                         std::vector<vtkBoneWidget*>& bones,
                         vtkQuaterniond& initialRotation)
{
  std::vector<double>::const_iterator valueIterator = values.begin();
  assert(channels.size() == bones.size());
  for (int i = 0; i < channels.size(); ++i)
    {
    rotations.push_back(
      GetParentToBoneRotation(
        valueIterator, channels[i], bones[i], initialRotation));
    }
}

} // end namespace


vtkStandardNewMacro(vtkBVHReader);

//----------------------------------------------------------------------------
vtkBVHReader::vtkBVHReader()
{
  this->FileName = "";
  this->RestArmatureIsValid = false;
  this->Armature = NULL;
  this->Frame = 0;
  this->NumberOfFrames = 0;
  this->FrameRate = 0;
  this->LinkToFirstChild = false;
  this->InitialRotation = vtkTransform::New();
  this->InitialRotation->RotateZ(180.0);
  this->InitialRotation->RotateX(90.0);

  this->SetNumberOfInputPorts(0);
}

//----------------------------------------------------------------------------
vtkBVHReader::~vtkBVHReader()
{
  this->InitialRotation->Delete();
  if (this->Armature)
    {
    this->Armature->Delete();
    }
}

//----------------------------------------------------------------------------
void vtkBVHReader::SetFileName(const char* filename)
{
  if (!filename && this->FileName == ""
    || filename && strcmp(this->FileName.c_str(), filename) == 0)
    {
    return;
    }

  this->FileName = filename ? filename : "";
  this->RestArmatureIsValid = false;
  this->Modified();
}

//----------------------------------------------------------------------------
const char* vtkBVHReader::GetFileName() const
{
  return this->FileName.c_str();
}

//----------------------------------------------------------------------------
void vtkBVHReader::SetFrame(unsigned int frame)
{
  if (this->Frame == frame)
    {
    return;
    }

  this->Frame = frame;
  if (this->RestArmatureIsValid)
    {
    this->ApplyFrameToArmature(this->GetArmature(), this->Frame);
    }
  this->Modified();
}

//----------------------------------------------------------------------------
void vtkBVHReader::SetLinkToFirstChild(bool link)
{
  if (this->LinkToFirstChild == link)
    {
    return;
    }

  this->LinkToFirstChild = link;
  if (this->LinkToFirstChild)
    {
    this->LinkBonesToFirstChild();
    }
  else
    {
    this->UnlinkBonesFromFirstChild();
    }
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkBVHReader::RequestData(vtkInformation *vtkNotUsed(request),
                              vtkInformationVector **vtkNotUsed(inputVector),
                              vtkInformationVector *outputVector)
{
  if (this->Armature && !this->RestArmatureIsValid)
    {
    this->InvalidReader();
    }

  // Create new armature if necessary
  if (!this->Armature)
    {
    this->Armature = vtkArmatureWidget::New();
    if (this->FileName == "")
      {
      vtkErrorMacro("A file name must be specified.");
      return 0;
      }
    }

  // open a BVH file for reading
  if (!this->RestArmatureIsValid)
    {
    std::ifstream file(this->FileName.c_str());
    if (!file.good())
      {
      vtkErrorMacro("Cannot open the given file.");
      return 0;
      }

    this->Bones.clear();
    this->Frames.clear();
    if (!this->Parse(file))
      {
      vtkErrorMacro("Error when parsing the file.");
      return 0;
      }

    file.close();
    }

  this->ApplyFrameToArmature(this->GetArmature(), this->Frame);

  vtkInformation* polydataInfo = outputVector->GetInformationObject(0);
  polydataInfo->Get(vtkDataObject::DATA_OBJECT())->DeepCopy(
    this->Armature->GetPolyData());

  return 1;
}

//----------------------------------------------------------------------------
int vtkBVHReader::Parse(std::ifstream& file)
{
  // Make sure first line is hierarchy
  std::string line = "";
  std::getline(file, line);
  if (MoveToNextKeyword(file, line) != "HIERARCHY")
    {
    std::cerr<<"Invalid BVH file, no hierarchy was specified." << std::endl;
    this->InvalidReader();
    return 0;
    }

  std::vector< std::vector<std::string> > channels;
  this->ParseRestArmature(file, -1, channels);
  this->RestArmatureIsValid = true;

  // By default, parse returns an armature where the bones are linked to
  // their first child. Unlink them if necessary.
  if (! this->LinkToFirstChild)
    {
    this->UnlinkBonesFromFirstChild();
    }

  this->ParseMotions(file, channels);

  return 1;
}

//----------------------------------------------------------------------------
void vtkBVHReader::ParseEndSite(std::ifstream& file, size_t parentId)
{
  std::string line;
  while(std::getline(file, line))
    {
    std::string keyword = MoveToNextKeyword(file, line);
    if (keyword == "}" || keyword == "MOTION")
      {
      return;
      }
    else if (keyword == "OFFSET")
      {
      // Offset is always set for the previous bone
      double pos[3];
      GetOffset(line, pos);
      this->TransformPoint(pos, pos);

      vtkBoneWidget* bone = this->Bones[parentId];

      double head[3];
      bone->GetWorldHeadRest(head);
      vtkMath::Add(pos, head, pos);

      bone->SetWorldTailRest(pos);
      }
    }
}

//----------------------------------------------------------------------------
void vtkBVHReader
::ParseRestArmature(std::ifstream& file,
                    size_t parentId,
                    std::vector< std::vector<std::string> >& channels)
{
  std::string line;
  while (std::getline(file, line))
    {
    std::string keyword = MoveToNextKeyword(file, line);
    if (keyword == "}" || keyword == "MOTION")
      {
      return;
      }
    else if (keyword == "OFFSET")
      {
      double pos[3];
      GetOffset(line, pos);
      this->TransformPoint(pos, pos);
      if (parentId == -1) // for root
        {
        this->Bones.front()->SetWorldHeadRest(pos);
        }
      else
        {
        // Offset is always set for the previous bone (except for root).
        vtkBoneWidget* bone = this->Bones[parentId];
        vtkBoneWidget* parent = this->Armature->GetBoneParent(bone);
        assert(parent);
        double parentHead[3];
        parent->GetWorldHeadRest(parentHead);
        vtkMath::Add(pos, parentHead, pos);

        // If the parent already has a child, that means that we need to
        // start this bone head at the given offset.
        if (this->Armature->GetBoneLinkedWithParent(bone))
          {
          parent->SetWorldTailRest(pos);
          }
        else
          {
          bone->SetWorldHeadRest(pos);
          }
        }
      }
    else if (keyword == "ROOT")
      {
      vtkBoneWidget* bone =
        this->Armature->CreateBone(NULL, GetBoneName(line, "ROOT"));

      this->Bones.push_back(bone);
      this->Armature->AddBone(bone);
      bone->Delete();
      }
    else if (keyword == "JOINT")
      {
      vtkBoneWidget* parent = NULL;
      if (parentId == -1) // first bone after root
        {
        parent = this->Bones.front();
        }
      else
        {
        parent = this->Bones[parentId];
        }

      vtkBoneWidget* bone =
        this->Armature->CreateBone(parent, GetBoneName(line));

      double head[3];
      parent->GetWorldHeadRest(head);
      bone->SetWorldTailRest(head[0], head[1] + 1.0, head[2]);

      // The head of the bone's first child's defines its tail position,
      // which means that:
      // - for parent that don't have a child, they are linked to their parent
      // - for parent that already have a child, the child needs to start
      // at an offset from its parent. So it can't be linked to it.
      this->Bones.push_back(bone);
      bool parentHasAtLeastAChild =
        this->Armature->CountDirectBoneChildren(parent) > 0;
      this->Armature->AddBone(bone, parent, !parentHasAtLeastAChild);
      bone->Delete();

      ParseRestArmature(file, this->Bones.size() - 1, channels);
      }
    else if (keyword == "End")
      {
      ParseEndSite(file, this->Bones.size() - 1);
      }
    else if (keyword == "CHANNELS")
      {
      channels.push_back( std::vector<std::string>() );
      GetChannels(line, channels.back());
      }
    }
}

//----------------------------------------------------------------------------
void vtkBVHReader
::ParseMotions(std::ifstream& file,
               std::vector< std::vector<std::string> >& channels)
{
  std::string line;
  std::getline(file, line);
  while (MoveToNextKeyword(file, line) != "MOTION")
    {
    if(!std::getline(file, line))
      {
      std::cerr<<"There was an error during the processing."<< std::endl;
      this->InvalidReader();
      return;
      }
    }

  this->Armature->SetWidgetState(vtkArmatureWidget::Pose);

  double wxyz[4];
  this->InitialRotation->GetOrientationWXYZ(wxyz);

  vtkQuaterniond InitialRotation;
  InitialRotation.SetRotationAngleAndAxis(
    vtkMath::RadiansFromDegrees(wxyz[0]), wxyz[1], wxyz[2], wxyz[3]);

  while (std::getline(file, line))
    {
    std::string keyword = MoveToNextKeyword(file, line);
    if (keyword == "Frames:")
      {
      this->NumberOfFrames = GetValue<unsigned int>(line, keyword);
      }
    else if (keyword == "Frame") // For "Frame time:"
      {
      this->FrameRate = GetValue<double>(line, "Frame time:");
      }
    else
      {
      // \todo (?) incorporate root translation
      std::vector<double> values;
      GetValues<double>(line, values);

      this->Frames.push_back(std::vector<vtkQuaterniond>());
      GetParentToBoneRotations(
        values, channels, this->Frames.back(), this->Bones, InitialRotation);
      }
    }
}

//----------------------------------------------------------------------------
bool vtkBVHReader
::ApplyFrameToArmature(vtkArmatureWidget* armature, unsigned int frame)
{
  if (!armature)
    {
    return false;
    }

  unsigned int numberOfFrames = static_cast<unsigned int>(this->Frames.size());
  if (numberOfFrames <= frame)
    {
    std::cerr<<"The input frame exceeds the total number of frames."<<std::endl
      <<" -> Defaulting to the last frame."<<std::endl;
    frame = numberOfFrames;
    }

  this->Armature->ResetPoseToRest();
  int oldState = this->Armature->SetWidgetState(vtkArmatureWidget::Pose);

  assert(this->Frames[this->Frame].size() == this->Bones.size());
  try
    {
    for (size_t i = 0; i < this->Bones.size(); ++i)
      {
      double axis[3];
      double angle =
        this->Frames.at(this->Frame).at(i).GetRotationAngleAndAxis(axis);
      this->Bones.at(i)->RotateTailWithParentWXYZ(angle, axis);
      }
    }
  catch(const std::out_of_range& oor)
    {
    std::cerr<<"Error while trying to set the pose #"<<frame
      <<" to the armature."<<std::endl<<"Make sure the armature is the same"
      <<"than the armature read by this reader."<<std::endl;
    return false;
    }

  this->Armature->SetWidgetState(oldState);

  return true;
}

//----------------------------------------------------------------------------
vtkQuaterniond vtkBVHReader
::GetParentToBoneRotation(unsigned int frame, unsigned int boneId)
{
  return this->Frames[frame][boneId];
}

//----------------------------------------------------------------------------
void vtkBVHReader::SetInitialRotation(vtkTransform* transform)
{
  if (!transform || transform == this->InitialRotation)
    {
    return;
    }

  double wxyz[4];
  transform->GetOrientationWXYZ(wxyz);

  this->InitialRotation->Identity();
  this->InitialRotation->RotateWXYZ(wxyz[0], wxyz[1], wxyz[2], wxyz[3]);

  this->RestArmatureIsValid = false;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkTransform* vtkBVHReader::GetInitialRotation() const
{
  return this->InitialRotation;
}

//----------------------------------------------------------------------------
void vtkBVHReader::UnlinkBonesFromFirstChild()
{
  if (!this->RestArmatureIsValid)
    {
    return;
    }

  int oldState = this->Armature->SetWidgetState(vtkArmatureWidget::Rest);

  for (BonesList::iterator it = this->Bones.begin();
    it != this->Bones.end(); ++it)
    {
    vtkNew<vtkCollection> children;
    this->Armature->FindBoneChildren(children.GetPointer(), *it);
    if (children->GetNumberOfItems() > 1)
      {
      double tail[3] = {0.0, 0.0, 0.0};

      for (int i = 0; i < children->GetNumberOfItems(); ++i)
        {
        vtkBoneWidget* child =
          vtkBoneWidget::SafeDownCast(children->GetItemAsObject(i));
        assert(child);

        this->Armature->SetBoneLinkedWithParent(child, false);

        double head[3];
        child->GetWorldHeadRest(head);

        vtkMath::Add(tail, head, tail);
        }

      vtkMath::MultiplyScalar(tail, 1.0 / children->GetNumberOfItems());
      (*it)->SetWorldTailRest(tail);
      }
    }

  this->Armature->SetWidgetState(oldState);
}

//----------------------------------------------------------------------------
void vtkBVHReader::LinkBonesToFirstChild()
{
  if (!this->RestArmatureIsValid)
    {
    return;
    }

  int oldState = this->Armature->GetWidgetState();
  this->Armature->SetWidgetState(vtkArmatureWidget::Rest);

  for (BonesList::iterator it = this->Bones.begin();
    it != this->Bones.end(); ++it)
    {
    vtkNew<vtkCollection> children;
    this->Armature->FindBoneChildren(children.GetPointer(), *it);
    if (children->GetNumberOfItems() > 1)
      {
      vtkBoneWidget* child =
        vtkBoneWidget::SafeDownCast(children->GetItemAsObject(0));
      assert(child);

      double head[3];
      child->GetWorldHeadRest(head);
      (*it)->SetWorldTailRest(head);

      this->Armature->SetBoneLinkedWithParent(child, true);
      }
    }

  this->Armature->SetWidgetState(oldState);
}

//----------------------------------------------------------------------------
int vtkBVHReader::CanReadFile(const char *filename)
{
  std::ifstream file(filename);
  if (!file.good())
    {
    return 0;
    }

  std::string name = filename;
  if (name.size() < 3 || name.substr(name.size() - 3) != "bvh")
    {
    return 0;
    }

  return 1;
}

//----------------------------------------------------------------------------
void vtkBVHReader::InvalidReader()
{
  this->RestArmatureIsValid = false;
  if (this->Armature)
    {
    this->Armature->Delete();
    this->Armature = NULL;
    }

  this->Frames.clear();
  this->Bones.clear();

  this->Frame = 0;
  this->NumberOfFrames = 0;
  this->FrameRate = 0;
  this->LinkToFirstChild = false;
}

//----------------------------------------------------------------------------
double* vtkBVHReader::TransformPoint(double point[3]) const
{
  return this->InitialRotation->TransformDoublePoint(point);
}

//----------------------------------------------------------------------------
void vtkBVHReader::TransformPoint(double point[3], double newPoint[3]) const
{
  double* p = this->TransformPoint(point);
  for (int i = 0; i < 3; ++i)
    {
    newPoint[i] = p[i];
    }
}

//----------------------------------------------------------------------------
void vtkBVHReader::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "File Name: " << this->FileName << "\n";
  os << indent << "Frame: " << this->Frame << "\n";
  os << indent << "LinkWithFirstChild: " << this->LinkToFirstChild << "\n";
  os << indent << "NumberOfFrames: " << this->NumberOfFrames << "\n";
  os << indent << "FrameRate: " << this->FrameRate << "\n";
}
