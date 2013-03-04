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

// MRMLDisplayableManager includes
#include "vtkMRMLArmatureDisplayableManager.h"

// Armatures includes
#include <vtkMRMLArmatureNode.h>
#include <vtkMRMLBoneDisplayNode.h>
#include <vtkMRMLBoneNode.h>

// Bender includes
#include <vtkArmatureRepresentation.h>
#include <vtkArmatureWidget.h>
#include <vtkBoneRepresentation.h>
#include <vtkDoubleConeBoneRepresentation.h>
#include <vtkBoneWidget.h>

// MRML includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkMRMLAnnotationClickCounter.h>
#include <vtkMRMLAnnotationDisplayableManagerHelper.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkColor.h>
#include <vtkHandleRepresentation.h>
#include <vtkHandleWidget.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkThreeDViewInteractorStyle.h>
#include <vtkTransform.h>

// STD includes
#include <algorithm>
#include <cmath>

//---------------------------------------------------------------------------
vtkStandardNewMacro(vtkMRMLArmatureDisplayableManager);
vtkCxxRevisionMacro(vtkMRMLArmatureDisplayableManager, "$Revision: 1 $");

//---------------------------------------------------------------------------
class vtkMRMLArmatureDisplayableManager::vtkInternal
{
public:
  typedef std::map<vtkMRMLArmatureNode*, vtkArmatureWidget*> ArmatureNodesLink;
  typedef std::map<vtkMRMLBoneNode*, vtkBoneWidget*> BoneNodesLink;

  vtkInternal(vtkMRMLArmatureDisplayableManager* external);
  ~vtkInternal();

  // View
  vtkMRMLAbstractViewNode* GetViewNode();

  // Armatures
  void AddArmatureNode(vtkMRMLArmatureNode*);
  void RemoveArmatureNode(vtkMRMLArmatureNode*);
  void RemoveArmatureNode(ArmatureNodesLink::iterator);
  void RemoveAllArmatureNodes();
  void UpdateArmatureNodeFromWidget(vtkMRMLArmatureNode* armatureNode,
                                    vtkArmatureWidget* widget);
  void UpdateArmatureWidgetFromNode(vtkMRMLArmatureNode* armatureNode,
                                    vtkArmatureWidget* armatureWidget);
  void UpdateArmatureNodes();

  vtkMRMLArmatureNode* GetArmatureNode(vtkArmatureWidget* armatureWidget);
  vtkMRMLArmatureNode* GetArmatureNode(vtkMRMLBoneNode* boneNode);

  // Bones
  void AddBoneNode(vtkMRMLBoneNode* bone);
  void RemoveBoneNode(vtkMRMLBoneNode* bone);
  void RemoveBoneNode(BoneNodesLink::iterator boneIt);
  void RemoveAllBoneNodes(vtkMRMLArmatureNode* armature);
  void UpdateBoneNodeFromWidget(vtkMRMLBoneNode* boneNode,
                                vtkBoneWidget* widget);
  void UpdateBoneWidgetFromNode(vtkMRMLBoneNode* boneNode,
                                vtkBoneWidget* boneWidget);

  void UpdateBoneNodes(vtkMRMLArmatureNode* armature);
  vtkMRMLBoneNode* GetBoneNode(vtkBoneWidget* boneWidget);
  vtkMRMLBoneNode* GetBoneParentNode(vtkMRMLBoneNode* boneNode);

  // Display
  void UpdateBoneDisplayNodeFromWidget(vtkMRMLBoneDisplayNode* boneDisplayNode,
                                       vtkBoneWidget* widget);

  // Widgets
  vtkArmatureWidget* CreateArmatureWidget();
  vtkBoneWidget* CreateBoneWidget();
  vtkArmatureWidget* GetArmatureWidget(vtkMRMLArmatureNode*);
  vtkBoneWidget* GetBoneWidget(vtkMRMLBoneNode*);

  ArmatureNodesLink ArmatureNodes;
  BoneNodesLink BoneNodes;
  vtkMRMLArmatureDisplayableManager* External;
  vtkMRMLArmatureNode* SelectedArmatureNode;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLArmatureDisplayableManager::vtkInternal
::vtkInternal(vtkMRMLArmatureDisplayableManager* external)
{
  this->External = external;
  this->SelectedArmatureNode = 0;
}

//---------------------------------------------------------------------------
vtkMRMLArmatureDisplayableManager::vtkInternal::~vtkInternal()
{
  this->RemoveAllArmatureNodes();
}

//---------------------------------------------------------------------------
vtkMRMLAbstractViewNode* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetViewNode()
{
  return vtkMRMLAbstractViewNode::SafeDownCast(
    this->External->GetMRMLDisplayableNode());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::AddArmatureNode(vtkMRMLArmatureNode* armatureNode)
{
  if (!armatureNode ||
      this->ArmatureNodes.find(vtkMRMLArmatureNode::SafeDownCast(armatureNode)) !=
      this->ArmatureNodes.end())
    {
    return;
    }

  // We associate the node with the widget if an instantiation is called.
  armatureNode->AddObserver(vtkCommand::ModifiedEvent,
                           this->External->GetMRMLNodesCallbackCommand());

  // We add the armatureNode without instantiating the widget first.
  this->ArmatureNodes.insert(
    std::pair<vtkMRMLArmatureNode*, vtkArmatureWidget*>(
      armatureNode, static_cast<vtkArmatureWidget*>(0)));
  // The armature widget is created here if needed.
  this->UpdateArmatureWidgetFromNode(armatureNode, 0);

  armatureNode->SetSelected(1);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::AddBoneNode(vtkMRMLBoneNode* boneNode)
{
  if (!boneNode ||
      this->BoneNodes.find(vtkMRMLBoneNode::SafeDownCast(boneNode)) !=
      this->BoneNodes.end())
    {
    return;
    }

  // We associate the node with the widget if an instantiation is called.
  boneNode->AddObserver(vtkCommand::ModifiedEvent,
                        this->External->GetMRMLNodesCallbackCommand());

  // Also observe the events emited by the displayable node.
  boneNode->AddObserver(vtkMRMLDisplayableNode::DisplayModifiedEvent,
                        this->External->GetMRMLNodesCallbackCommand());

  // We add the boneNode without instantiating the widget first.
  this->BoneNodes.insert(
    std::pair<vtkMRMLBoneNode*, vtkBoneWidget*>(
      boneNode, static_cast<vtkBoneWidget*>(0)));
  // The bone widget is created here if needed.
  this->UpdateBoneWidgetFromNode(boneNode, 0);

  vtkMRMLArmatureNode* armatureNode = this->GetArmatureNode(boneNode);
  vtkMRMLBoneNode* boneParentNode = this->GetBoneParentNode(boneNode);

  vtkBoneWidget* boneWidget = this->GetBoneWidget(boneNode);
  vtkBoneWidget* parentBoneWidget = this->GetBoneWidget(boneParentNode);
  vtkArmatureWidget* armatureWidget = this->GetArmatureWidget(armatureNode);

  if (armatureWidget && boneWidget && !armatureWidget->HasBone(boneWidget))
    {
    armatureWidget->UpdateBoneWithArmatureOptions(
      boneWidget,
      parentBoneWidget);
    vtkMRMLBoneDisplayNode* boneDisplayNode = boneNode->GetBoneDisplayNode();
    if (boneDisplayNode)
      {
      boneDisplayNode->SetOpacity(armatureNode->GetOpacity());
      double rgb[3];
      armatureNode->GetColor(rgb);
      boneDisplayNode->SetColor(rgb);
      }

    boneWidget->SetShowParenthood(parentBoneWidget != 0);
    boneNode->SetHasParent(parentBoneWidget != 0);

    armatureWidget->AddBone(
      boneWidget, parentBoneWidget, boneNode->GetBoneLinkedWithParent());
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveArmatureNode(vtkMRMLArmatureNode* armatureNode)
{
  if (!armatureNode)
    {
    return;
    }

  bool wasSelected = armatureNode == this->SelectedArmatureNode;
  if (wasSelected)
    {
    this->SelectedArmatureNode = 0;
    }

  this->RemoveArmatureNode(this->ArmatureNodes.find(armatureNode));

  if (wasSelected && !this->ArmatureNodes.empty())
    {
    this->ArmatureNodes.begin()->first->SetSelected(1);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveArmatureNode(ArmatureNodesLink::iterator it)
{
  if (it == this->ArmatureNodes.end())
    {
    return;
    }

  // The manager has the responsabilty to delete the widget.
  if (it->second)
    {
    it->second->Delete();
    it->second = 0;
    }
  this->RemoveAllBoneNodes(it->first);

  // TODO: it->first might have already been deleted
  it->first->RemoveObserver(this->External->GetMRMLNodesCallbackCommand());
  this->ArmatureNodes.erase(it);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveAllArmatureNodes()
{
  // The manager has the responsabilty to delete the widgets.
  while (this->ArmatureNodes.size() > 0)
    {
    this->RemoveArmatureNode(this->ArmatureNodes.begin());
    }
  this->ArmatureNodes.clear();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveAllBoneNodes(vtkMRMLArmatureNode* armatureNode)
{
  if (armatureNode == 0)
    {
    return;
    }

  vtkNew<vtkCollection> bones;
  armatureNode->GetAllBones(bones.GetPointer());
  vtkCollectionSimpleIterator it;
  vtkMRMLNode* node = 0;
  for (bones->InitTraversal(it);
       (node = vtkMRMLNode::SafeDownCast(
         bones->GetNextItemAsObject(it)));)
    {
    vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
    if (boneNode)
      {
      this->External->GetMRMLScene()->RemoveNode(boneNode);
      }
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveBoneNode(vtkMRMLBoneNode* boneNode)
 {
  if (!boneNode)
    {
    return;
    }

  vtkBoneWidget* boneWidget = this->GetBoneWidget(boneNode);
  for (ArmatureNodesLink::iterator it = this->ArmatureNodes.begin();
    it != this->ArmatureNodes.end(); ++it)
    {
    if (it->second && it->second->HasBone(boneWidget))
      {
      it->second->RemoveBone(boneWidget);
      }
    }

  this->RemoveBoneNode(this->BoneNodes.find(boneNode));
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveBoneNode(BoneNodesLink::iterator it)
{
  if (it == this->BoneNodes.end())
    {
    return;
    }

  // The manager has the responsabilty to delete the widget.
  if (it->second)
    {
    it->second->Delete();
    }

  // TODO: it->first might have already been deleted
  it->first->RemoveObserver(this->External->GetMRMLNodesCallbackCommand());
  this->BoneNodes.erase(it);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal::UpdateArmatureNodes()
{
  if (this->External->GetMRMLScene() == 0)
    {
    this->RemoveAllArmatureNodes();
    return;
    }

  vtkMRMLNode* node;
  vtkCollectionSimpleIterator it;
  vtkCollection* scene = this->External->GetMRMLScene()->GetNodes();
  for (scene->InitTraversal(it);
       (node = vtkMRMLNode::SafeDownCast(scene->GetNextItemAsObject(it))) ;)
    {
    vtkMRMLArmatureNode* armatureNode = vtkMRMLArmatureNode::SafeDownCast(node);
    if (armatureNode)
      {
      this->AddArmatureNode(armatureNode);
      }
    }
}

//---------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetArmatureNode(vtkArmatureWidget* armatureWidget)
{
  if (!armatureWidget)
    {
    return 0;
    }

  vtkMRMLArmatureNode* armatureNode = 0;
  for (ArmatureNodesLink::iterator it=this->ArmatureNodes.begin();
       it != this->ArmatureNodes.end(); ++it)
    {
    if (it->second == armatureWidget)
      {
      armatureNode = it->first;
      break;
      }
    }

  return armatureNode;
}

//---------------------------------------------------------------------------
vtkMRMLArmatureNode* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetArmatureNode(vtkMRMLBoneNode* boneNode)
{
  if (!boneNode)
    {
    return 0;
    }

  vtkMRMLAnnotationHierarchyNode* hierarchyNode
    = vtkMRMLAnnotationHierarchyNode::SafeDownCast(
        vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
            boneNode->GetScene(), boneNode->GetID()));

  vtkMRMLArmatureNode* armatureNode = 0;
  if (hierarchyNode && hierarchyNode->GetTopParentNode())
    {
    armatureNode = vtkMRMLArmatureNode::SafeDownCast(
      hierarchyNode->GetTopParentNode());
    }

  return armatureNode;
}

//---------------------------------------------------------------------------
vtkMRMLBoneNode* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetBoneNode(vtkBoneWidget* boneWidget)
{
  if (!boneWidget)
    {
    return 0;
    }

  vtkMRMLBoneNode* boneNode = 0;
  for (BoneNodesLink::iterator it = this->BoneNodes.begin();
       it != this->BoneNodes.end(); ++it)
    {
    if (it->second == boneWidget)
      {
      boneNode = it->first;
      break;
      }
    }

  return boneNode;
}

//---------------------------------------------------------------------------
vtkMRMLBoneNode* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetBoneParentNode(vtkMRMLBoneNode* boneNode)
{
  if (boneNode)
    {
    vtkMRMLAnnotationHierarchyNode* hierarchyNode
      = vtkMRMLAnnotationHierarchyNode::SafeDownCast(
          vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
              boneNode->GetScene(), boneNode->GetID()));

    if (hierarchyNode && hierarchyNode->GetParentNode())
      {
      return vtkMRMLBoneNode::SafeDownCast(
        hierarchyNode->GetParentNode()->GetAssociatedNode());
      }
    }

  return NULL;
}

//---------------------------------------------------------------------------
vtkArmatureWidget* vtkMRMLArmatureDisplayableManager::vtkInternal
::CreateArmatureWidget()
{
  // Instantiate armature widget and its representation
  vtkNew<vtkArmatureRepresentation> rep;
  double defaultBounds[6] = {100, -100, 100, -100, 100, -100};
  rep->PlaceWidget(defaultBounds);
  //rep->SetOutlineTranslation(0);
  //rep->SetScaleEnabled(0);
  //rep->SetDrawPlane(0);

  // The Manager has to manage the destruction of the widgets
  vtkArmatureWidget* armatureWidget = vtkArmatureWidget::New();
  armatureWidget->SetInteractor(this->External->GetInteractor());
  armatureWidget->SetRepresentation(rep.GetPointer());
  armatureWidget->SetEnabled(0);

  vtkNew<vtkDoubleConeBoneRepresentation> newRep;
  armatureWidget->SetBonesRepresentation(newRep.GetPointer());

  // Link widget evenement to the LogicCallbackCommand
  armatureWidget->AddObserver(vtkCommand::StartInteractionEvent,
                              this->External->GetWidgetsCallbackCommand());
  armatureWidget->AddObserver(vtkCommand::InteractionEvent,
                              this->External->GetWidgetsCallbackCommand());
  armatureWidget->AddObserver(vtkCommand::EndInteractionEvent,
                              this->External->GetWidgetsCallbackCommand());
  armatureWidget->AddObserver(vtkCommand::UpdateEvent,
                              this->External->GetWidgetsCallbackCommand());

  return armatureWidget;
}


//---------------------------------------------------------------------------
vtkBoneWidget* vtkMRMLArmatureDisplayableManager::vtkInternal
::CreateBoneWidget()
{
  // Instantiate bone widget and its representation
  //vtkNew<vtkBoneRepresentation> rep;
  //double defaultBounds[6] = {100, -100, 100, -100, 100, -100};
  //rep->PlaceWidget(defaultBounds);
  //rep->SetOutlineTranslation(0);
  //rep->SetScaleEnabled(0);
  //rep->SetDrawPlane(0);

  // The Manager has to manage the destruction of the widgets
  vtkBoneWidget* boneWidget = vtkBoneWidget::New();
  boneWidget->SetInteractor(this->External->GetInteractor());
  vtkNew<vtkDoubleConeBoneRepresentation> boneRepresentation;
  boneWidget->SetRepresentation(boneRepresentation.GetPointer());
  boneWidget->SetEnabled(0);
  boneWidget->SetWidgetStateToRest();

  // Link widget evenement to the LogicCallbackCommand
  boneWidget->AddObserver(vtkCommand::StartInteractionEvent,
                          this->External->GetWidgetsCallbackCommand());
  boneWidget->AddObserver(vtkCommand::InteractionEvent,
                          this->External->GetWidgetsCallbackCommand());
  boneWidget->AddObserver(vtkCommand::EndInteractionEvent,
                          this->External->GetWidgetsCallbackCommand());
  boneWidget->AddObserver(vtkCommand::UpdateEvent,
                          this->External->GetWidgetsCallbackCommand());

  boneWidget->GetBoneRepresentation()->AddObserver(vtkCommand::ModifiedEvent,
    this->External->GetWidgetsCallbackCommand());

  return boneWidget;
}

//---------------------------------------------------------------------------
vtkArmatureWidget* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetArmatureWidget(vtkMRMLArmatureNode* armatureNode)
{
  if (!armatureNode)
    {
    return 0;
    }

  ArmatureNodesLink::iterator it = this->ArmatureNodes.find(armatureNode);
  return (it != this->ArmatureNodes.end()) ? it->second : 0;
}

//---------------------------------------------------------------------------
vtkBoneWidget* vtkMRMLArmatureDisplayableManager::vtkInternal
::GetBoneWidget(vtkMRMLBoneNode* boneNode)
{
  if (!boneNode)
    {
    return 0;
    }

  BoneNodesLink::iterator it = this->BoneNodes.find(boneNode);
  return (it != this->BoneNodes.end()) ? it->second : 0;
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateArmatureWidgetFromNode(vtkMRMLArmatureNode* armatureNode,
                               vtkArmatureWidget* armatureWidget)
{
  if (!armatureNode)//|| (!armatureWidget && !armatureNode->GetWidgetVisible()))
    {
    return;
    }

  if (armatureNode->GetSelected()
    && armatureNode != this->SelectedArmatureNode)
    {
    if (this->SelectedArmatureNode)
      {
      this->SelectedArmatureNode->SetSelected(0);
      }
    this->SelectedArmatureNode = armatureNode;
    }

  if (!armatureWidget)
    {
    // Instantiate widget and link it if
    // there is no one associated to the armatureNode yet
    armatureWidget = this->CreateArmatureWidget();
    this->ArmatureNodes.find(armatureNode)->second = armatureWidget;
    }

  armatureNode->PasteArmatureNodeProperties(armatureWidget);
  armatureNode->SetArmaturePolyData(armatureWidget->GetPolyData());

  armatureWidget->SetEnabled(1);//armatureNode->GetWidgetVisible());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateBoneWidgetFromNode(vtkMRMLBoneNode* boneNode,
                           vtkBoneWidget* boneWidget)
{
  if (!boneNode)
    {
    return;
    }

  if (!boneWidget)
    {
    // Instantiate widget and link it if
    // there is no one associated to the boneNode yet
    boneWidget = this->CreateBoneWidget();
    this->BoneNodes.find(boneNode)->second = boneWidget;
    }

  vtkMRMLBoneDisplayNode* boneDisplayNode =
    boneNode ? boneNode->GetBoneDisplayNode() : 0;

  // We need to stop listening to the boneWidget when changing its properties
  // according to the node. Otherwise it will send ModifiedEvent() for the
  // first property changed, thus updating the node.
  // -> The widget won't be properly updated if more than 1 property
  // is changed.
  // Hackish solution, remove observer then add it again
  boneWidget->RemoveObservers(vtkCommand::ModifiedEvent,
    this->External->GetWidgetsCallbackCommand());
  boneNode->PasteBoneNodeProperties(boneWidget);
  if (boneDisplayNode)
    {
    boneDisplayNode->PasteBoneDisplayNodeProperties(boneWidget);
    }
  boneWidget->AddObserver(vtkCommand::ModifiedEvent,
    this->External->GetWidgetsCallbackCommand());

  vtkMRMLArmatureNode* armatureNode = this->GetArmatureNode(boneNode);
  if (armatureNode)
    {
    vtkArmatureWidget* armatureWidget = this->GetArmatureWidget(armatureNode);

    if (armatureWidget && armatureWidget->HasBone(boneWidget))
      {
      armatureWidget->SetBoneLinkedWithParent(boneWidget,
        boneNode->GetBoneLinkedWithParent());
      }
    }

  bool visible = (boneDisplayNode ?
     boneDisplayNode->GetVisibility(this->GetViewNode()->GetID()) : false);
  boneWidget->SetEnabled(visible);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateArmatureNodeFromWidget(vtkMRMLArmatureNode* armatureNode,
                               vtkArmatureWidget* widget)
{
  int wasModifying = armatureNode->StartModify();
  armatureNode->CopyArmatureWidgetProperties(widget);

  vtkNew<vtkCollection> bones;
  armatureNode->GetAllBones(bones.GetPointer());
  for (int i = 0; i < bones->GetNumberOfItems(); ++i)
    {
    vtkMRMLBoneNode* boneNode =
      vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));
    vtkBoneWidget* boneWidget = this->GetBoneWidget(boneNode);

    if (boneNode && boneWidget && widget->HasBone(boneWidget))
      {
      boneNode->SetBoneLinkedWithParent(
        widget->GetBoneLinkedWithParent(boneWidget));
      boneNode->SetHasParent(widget->GetBoneParent(boneWidget) != 0);
      }
    }

  armatureNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateBoneNodeFromWidget(vtkMRMLBoneNode* boneNode,
                           vtkBoneWidget* widget)
{
  if (!boneNode)
    {
    return;
    }
  int wasModifying = boneNode->StartModify();
  boneNode->CopyBoneWidgetProperties(widget);
  boneNode->EndModify(wasModifying);

  this->UpdateBoneDisplayNodeFromWidget(boneNode->GetBoneDisplayNode(), widget);

  vtkMRMLArmatureNode* armatureNode = this->GetArmatureNode(boneNode);
  if (armatureNode && boneNode->GetSelected())
    {
    bool shouldSelectBone = true;
    if (boneNode->GetSelected() == vtkBoneWidget::TailSelected)
      {
      vtkMRMLAnnotationHierarchyNode* hierarchyNode
        = vtkMRMLAnnotationHierarchyNode::SafeDownCast(
          vtkMRMLHierarchyNode::GetAssociatedHierarchyNode(
              boneNode->GetScene(), boneNode->GetID()));
      if (hierarchyNode)
        {
        std::vector<vtkMRMLHierarchyNode*> children =
          hierarchyNode->GetChildrenNodes();
        for (std::vector<vtkMRMLHierarchyNode*>::iterator it =
          children.begin(); it != children.end(); ++it)
          {
          vtkMRMLBoneNode* boneChild =
            vtkMRMLBoneNode::SafeDownCast((*it)->GetAssociatedNode());
          if (boneChild && boneChild->GetBoneLinkedWithParent())
            {
            vtkMRMLBoneDisplayNode* boneDisplayNode =
              boneChild->GetBoneDisplayNode();
            if (boneDisplayNode && boneDisplayNode->GetSelected())
              {
              shouldSelectBone = false;
              break;
              }
            }
          }
        }
      }

    if (shouldSelectBone)
      {
      armatureNode->InvokeEvent(
        vtkMRMLArmatureNode::ArmatureBoneModified, boneNode->GetID());
      }
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateBoneDisplayNodeFromWidget(vtkMRMLBoneDisplayNode* boneDisplayNode,
                                  vtkBoneWidget* widget)
{
  if (!boneDisplayNode)
    {
    return;
    }
  int wasModifying = boneDisplayNode->StartModify();
  boneDisplayNode->CopyBoneWidgetDisplayProperties(widget);
  boneDisplayNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
// vtkMRMLSliceModelDisplayableManager methods

//---------------------------------------------------------------------------
vtkMRMLArmatureDisplayableManager::vtkMRMLArmatureDisplayableManager()
{
  this->Internal = new vtkInternal(this);
  this->m_Focus = "vtkMRMLArmatureNode";

  //std::cout<<"vtkMRMLArmatureDisplayableManager Created !"<<std::endl;
}

//---------------------------------------------------------------------------
vtkMRMLArmatureDisplayableManager::~vtkMRMLArmatureDisplayableManager()
{
  delete this->Internal;
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::UnobserveMRMLScene()
{
  this->Internal->RemoveAllArmatureNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::UpdateFromMRMLScene()
{
  this->Internal->UpdateArmatureNodes();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::OnMRMLSceneNodeAdded(vtkMRMLNode* nodeAdded)
{
  if (!this->IsManageable(nodeAdded))
    {
    return;
    }

  if (nodeAdded->IsA("vtkMRMLArmatureNode"))
    {
    this->Internal->AddArmatureNode(vtkMRMLArmatureNode::SafeDownCast(nodeAdded));
    }
  else if (nodeAdded->IsA("vtkMRMLBoneNode"))
    {
    vtkMRMLBoneNode* newBoneNode = vtkMRMLBoneNode::SafeDownCast(nodeAdded);
    this->Internal->AddBoneNode(newBoneNode);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::OnMRMLSceneNodeAboutToBeRemoved(vtkMRMLNode* nodeRemoved)
{
  if (!this->IsManageable(nodeRemoved)
    || this->GetMRMLScene()->IsBatchProcessing())
    {
    return;
    }

  if (nodeRemoved->IsA("vtkMRMLArmatureNode"))
    {
    this->Internal->RemoveAllBoneNodes(
      vtkMRMLArmatureNode::SafeDownCast(nodeRemoved));
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::OnMRMLSceneNodeRemoved(vtkMRMLNode* nodeRemoved)
{
  if (!this->IsManageable(nodeRemoved))
    {
    return;
    }

  if (nodeRemoved->IsA("vtkMRMLArmatureNode"))
    {
    this->Internal->RemoveArmatureNode(
      vtkMRMLArmatureNode::SafeDownCast(nodeRemoved));
    }
  else if (nodeRemoved->IsA("vtkMRMLBoneNode"))
    {
    this->Internal->RemoveBoneNode(
      vtkMRMLBoneNode::SafeDownCast(nodeRemoved));
    }
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::OnMRMLNodeModified(vtkMRMLNode* node)
{
  vtkMRMLArmatureNode* armatureNode = vtkMRMLArmatureNode::SafeDownCast(node);
  if (armatureNode)
    {
    vtkArmatureWidget* armatureWidget =
      this->Internal->GetArmatureWidget(armatureNode);
    this->Internal->UpdateArmatureWidgetFromNode(armatureNode, armatureWidget);
    }
  vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(node);
  if (boneNode)
    {
    vtkBoneWidget* boneWidget = this->Internal->GetBoneWidget(boneNode);
    this->Internal->UpdateBoneWidgetFromNode(boneNode, boneWidget);
    }
  this->RequestRender();
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  assert(newScene != this->GetMRMLScene());

  vtkNew<vtkIntArray> sceneEvents;
  //sceneEvents->InsertNextValue(vtkMRMLScene::StartBatchProcessEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::StartCloseEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::EndCloseEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::StartImportEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::EndImportEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::StartRestoreEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::EndRestoreEvent);
  //sceneEvents->InsertNextValue(vtkMRMLScene::NewSceneEvent);
  sceneEvents->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  sceneEvents->InsertNextValue(vtkMRMLScene::NodeAboutToBeRemovedEvent);
  sceneEvents->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);

  this->SetAndObserveMRMLSceneEventsInternal(newScene, sceneEvents.GetPointer());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::OnMRMLAnnotationNodeModifiedEvent(vtkMRMLNode* node)
{
  this->OnMRMLNodeModified(node);
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::ProcessWidgetsEvents(vtkObject *caller,
                       unsigned long vtkNotUsed(event),
                       void *vtkNotUsed(callData))
{
  vtkArmatureWidget* armatureWidget = vtkArmatureWidget::SafeDownCast(caller);
  if (armatureWidget)
    {
    vtkMRMLArmatureNode* armatureNode = Internal->GetArmatureNode(armatureWidget);
    if (!armatureNode)
      {
      return;
      }
    this->Internal->UpdateArmatureNodeFromWidget(armatureNode, armatureWidget);
    }
  vtkBoneWidget* boneWidget = vtkBoneWidget::SafeDownCast(caller);
  if (boneWidget)
    {
    vtkMRMLBoneNode* boneNode = Internal->GetBoneNode(boneWidget);
    if (!boneNode)
      {
      return;
      }
    this->Internal->UpdateBoneNodeFromWidget(boneNode, boneWidget);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::ProcessMRMLNodesEvents(vtkObject *caller,
                         unsigned long event,
                         void* callData)
{
  if (event == vtkMRMLDisplayableNode::DisplayModifiedEvent)
    {
    vtkMRMLBoneNode* boneNode = vtkMRMLBoneNode::SafeDownCast(caller);
    vtkBoneWidget* boneWidget = this->Internal->GetBoneWidget(boneNode);

    this->Internal->UpdateBoneWidgetFromNode(boneNode, boneWidget);
    this->RequestRender();
    }

  this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
}

//----------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::ProcessMRMLSceneEvents(vtkObject *caller,
                         unsigned long event,
                         void* callData)
{
  if (event == vtkMRMLScene::NodeAboutToBeRemovedEvent)
    {
    vtkMRMLNode* node = reinterpret_cast<vtkMRMLNode*>(callData);
    this->OnMRMLSceneNodeAboutToBeRemoved(node);
    }

  this->Superclass::ProcessMRMLSceneEvents(caller, event, callData);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::Create()
{
  this->Internal->UpdateArmatureNodes();
}

//---------------------------------------------------------------------------
bool vtkMRMLArmatureDisplayableManager::IsManageable(vtkMRMLNode* node)
{
  return this->Superclass::IsManageable(node) ||
    node->IsA("vtkMRMLBoneNode");
}

//---------------------------------------------------------------------------
bool vtkMRMLArmatureDisplayableManager::IsManageable(const char* nodeID)
{
  return this->Superclass::IsManageable(nodeID) ||
    (nodeID && !strcmp(nodeID, "vtkMRMLBoneNode"));
}

//---------------------------------------------------------------------------
/// Create a annotationMRMLnode
void vtkMRMLArmatureDisplayableManager
::OnClickInRenderWindow(double x, double y, const char *associatedNodeID)
{
  //std::cout<<"OnClickInRenderWindow"<<std::endl;
  if (!this->IsCorrectDisplayableManager())
    {
    return;
    }

  //std::cout<<"OnClickInRenderWindow"<<std::endl;

  // place the seed where the user clicked
  this->PlaceSeed(x,y);

  double worldCoordinates[4] = {0.,0.,0.,1.};
  this->GetDisplayToWorldCoordinates(x, y, worldCoordinates);

  // If there is a current armature
  if (this->Internal->SelectedArmatureNode)
    {
    // Look in the bones if ay of them is selected
    vtkNew<vtkCollection> bones;
    this->Internal->SelectedArmatureNode->GetAllBones(bones.GetPointer());
    vtkMRMLBoneNode* currentBone = 0;
    for (int i = 0; i < bones->GetNumberOfItems(); ++i)
      {
      vtkMRMLBoneNode* bone =
        vtkMRMLBoneNode::SafeDownCast(bones->GetItemAsObject(i));
      if (bone)
        {
        // We look at the display node because it's what the user sees
        vtkMRMLBoneDisplayNode* boneDisplayNode = bone->GetBoneDisplayNode();
        if (boneDisplayNode && boneDisplayNode->GetSelected())
          {
          currentBone = bone;
          }
        }
      }

    if (currentBone) // Means a bone is currently selected.
      {
      double head[3];
      currentBone->GetWorldHeadRest(head);
      this->CreateAndAddBoneToCurrentScene(
        head, worldCoordinates, associatedNodeID);
      return;
      }
    }
  else // No armature currently selected.
    {
    if (this->Internal->ArmatureNodes.empty()) // None exist
      {
      // So let's create one
      vtkNew<vtkMRMLArmatureNode> armatureNode;
      armatureNode->SetName(
        this->GetMRMLScene()->GetUniqueNameByString("Armature"));
      this->GetMRMLScene()->SaveStateForUndo();

      this->GetMRMLScene()->AddNode(armatureNode.GetPointer());
      }
    else // One already exists, so let's select one randomly
      {
      this->Internal->ArmatureNodes.begin()->first->SetSelected(1);
      }
    }

  if (this->m_ClickCounter->Click() >= 2)
    {
    this->CreateAndAddBoneToCurrentScene(
      this->LastClickWorldCoordinates, worldCoordinates, associatedNodeID);
    }
  memcpy(this->LastClickWorldCoordinates,
    worldCoordinates, 4 * sizeof(double));
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager
::CreateAndAddBoneToCurrentScene(double head[3], double tail[3],
                                 const char *associatedNodeID)
{
// switch to updating state to avoid events mess
  this->m_Updating = 1;

  vtkNew<vtkMRMLBoneNode> boneNode;
  boneNode->SetName(this->GetMRMLScene()->GetUniqueNameByString("Bone"));
  boneNode->SetWorldHeadRest(head);
  boneNode->SetWorldTailRest(tail);

  this->GetMRMLScene()->SaveStateForUndo();

  // is there a node associated with this?
  if (associatedNodeID)
    {
    vtkDebugMacro("Associate Node ID: " << associatedNodeID);
    boneNode->SetAttribute("AssociatedNodeID", associatedNodeID);
    }
  boneNode->Initialize(this->GetMRMLScene());

  // reset updating state
  this->m_Updating = 0;

  // if this was a one time place, go back to view transform mode
  vtkMRMLInteractionNode *interactionNode = this->GetInteractionNode();
  this->Helper->RemoveSeeds();
  this->m_ClickCounter->Reset();
  if (interactionNode && interactionNode->GetPlaceModePersistence() != 1)
    {
    interactionNode->SetCurrentInteractionMode(
      vtkMRMLInteractionNode::ViewTransform);
    }
}
