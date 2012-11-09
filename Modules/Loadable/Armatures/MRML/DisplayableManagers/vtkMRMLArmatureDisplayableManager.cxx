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

  // Widgets
  vtkArmatureWidget* CreateArmatureWidget();
  vtkBoneWidget* CreateBoneWidget();
  vtkArmatureWidget* GetArmatureWidget(vtkMRMLArmatureNode*);
  vtkBoneWidget* GetBoneWidget(vtkMRMLBoneNode*);

  ArmatureNodesLink ArmatureNodes;
  BoneNodesLink BoneNodes;
  vtkMRMLArmatureDisplayableManager* External;
};

//---------------------------------------------------------------------------
// vtkInternal methods

//---------------------------------------------------------------------------
vtkMRMLArmatureDisplayableManager::vtkInternal
::vtkInternal(vtkMRMLArmatureDisplayableManager* external)
{
  this->External = external;
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
  // We add the boneNode without instantiating the widget first.
  this->BoneNodes.insert(
    std::pair<vtkMRMLBoneNode*, vtkBoneWidget*>(
      boneNode, static_cast<vtkBoneWidget*>(0)));

  // The bone widget is created here if needed.
  this->UpdateBoneWidgetFromNode(boneNode, 0);

  /*if(this->ArmatureNodes.empty())
    {
    vtkNew<vtkMRMLArmatureNode> newArmatureNode;
    this->AddArmatureNode(newArmatureNode);


    }*/

}


//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveArmatureNode(vtkMRMLArmatureNode* armatureNode)
{
  if (!armatureNode)
    {
    return;
    }

  this->RemoveArmatureNode(this->ArmatureNodes.find(armatureNode));
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::RemoveArmatureNode(ArmatureNodesLink::iterator it)
{
  if (it == this->ArmatureNodes.end())
    {
    return;
    }

  this->RemoveAllBoneNodes(it->first);

  // The manager has the responsabilty to delete the widget.
  if (it->second)
    {
    it->second->Delete();
    }

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
      this->RemoveBoneNode(boneNode);
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

  armatureWidget->SetBonesRepresentation(vtkArmatureWidget::DoubleCone);

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
  // Need ModifiedEvent because the bone can be modified by the armature too
  // (And the node need to reflect that).
  boneWidget->AddObserver(vtkCommand::ModifiedEvent,
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

  if (!armatureWidget)
    {
    // Instantiate widget and link it if
    // there is no one associated to the armatureNode yet
    armatureWidget = this->CreateArmatureWidget();
    this->ArmatureNodes.find(armatureNode)->second = armatureWidget;
    }

  armatureNode->PasteArmatureNodeProperties(armatureWidget);

  armatureWidget->SetEnabled(1);//armatureNode->GetWidgetVisible());
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateBoneWidgetFromNode(vtkMRMLBoneNode* boneNode,
                           vtkBoneWidget* boneWidget)
{
  vtkMRMLBoneDisplayNode* boneDisplayNode =
    boneNode ? boneNode->GetBoneDisplayNode() : 0;
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

  // We need to stop listening to the boneWidget when changing its properties
  // according to the node. Otherwise it will send ModifiedEvent() for the
  // first property changed, thus updating the node.
  // -> The widget won't be properly updated if more than 1 property
  // is changed.
  // Hackish solution, remove observer then add it again
  boneWidget->RemoveObservers(vtkCommand::ModifiedEvent,
    this->External->GetWidgetsCallbackCommand());
  boneNode->PasteBoneNodeProperties(boneWidget);
  boneWidget->AddObserver(vtkCommand::ModifiedEvent,
    this->External->GetWidgetsCallbackCommand());

  bool visible = boneNode->GetVisible() &&
    (boneDisplayNode ?
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
  armatureNode->EndModify(wasModifying);
}

//---------------------------------------------------------------------------
void vtkMRMLArmatureDisplayableManager::vtkInternal
::UpdateBoneNodeFromWidget(vtkMRMLBoneNode* boneNode,
                           vtkBoneWidget* widget)
{
  int wasModifying = boneNode->StartModify();
  boneNode->CopyBoneWidgetProperties(widget);
  boneNode->EndModify(wasModifying);
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
  if (this->GetMRMLScene()->IsBatchProcessing() ||
      !this->IsManageable(nodeAdded))
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

    vtkMRMLAnnotationHierarchyNode* hierarchyNode
      = vtkMRMLAnnotationHierarchyNode::SafeDownCast(
          vtkMRMLHierarchyNode
            ::GetAssociatedHierarchyNode(
              newBoneNode->GetScene(), newBoneNode->GetID()));

    if (hierarchyNode
      && hierarchyNode->GetParentNode()
      && hierarchyNode->GetTopParentNode())
      {
      vtkMRMLBoneNode* boneParentNode
        = vtkMRMLBoneNode::SafeDownCast(
            hierarchyNode->GetParentNode()->GetAssociatedNode());

      vtkMRMLArmatureNode* armatureNode
        = vtkMRMLArmatureNode::SafeDownCast(
            hierarchyNode->GetTopParentNode());

      vtkBoneWidget* newBoneWidget
        = this->Internal->GetBoneWidget(newBoneNode);
      vtkBoneWidget* parentBoneWidget
        = this->Internal->GetBoneWidget(boneParentNode);
      vtkArmatureWidget* armatureWidget
        = this->Internal->GetArmatureWidget(armatureNode);

      if (armatureWidget
        && !armatureWidget->HasBone(newBoneWidget))
        {
        armatureWidget->UpdateBoneWithArmatureOptions(
          newBoneWidget,
          parentBoneWidget);

        armatureWidget->AddBone(newBoneWidget, parentBoneWidget);
        }
      }
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
void vtkMRMLArmatureDisplayableManager::OnClickInRenderWindow(double x, double y, const char *associatedNodeID)
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

  if (this->m_ClickCounter->Click() >= 2)
    {
    // switch to updating state to avoid events mess
    this->m_Updating = 1;

    vtkNew<vtkMRMLBoneNode> boneNode;
    boneNode->SetName(this->GetMRMLScene()->GetUniqueNameByString("Bone"));
    boneNode->SetWorldHeadRest(this->LastClickWorldCoordinates);
    boneNode->SetWorldTailRest(worldCoordinates);

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
    if (interactionNode && interactionNode->GetPlaceModePersistence() != 1)
      {
      this->Helper->RemoveSeeds();
      this->m_ClickCounter->Reset();
      interactionNode->SetCurrentInteractionMode(vtkMRMLInteractionNode::ViewTransform);
      }
    }
  memcpy(this->LastClickWorldCoordinates, worldCoordinates, 4 * sizeof(double));
}
