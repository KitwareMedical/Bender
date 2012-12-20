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

#ifndef __vtkBoneWidget_h
#define __vtkBoneWidget_h

// .NAME vtkBoneWidget - Widget for skeletal animation
// .SECTION General Description
// The vtkBoneWidget is a widget meant for animation. It defines a bone with
// a head and a tail point. The head and the tail can be manipulated by the
// user depending on the widget current mode. The widget has 4 modes in which
// its behavior varies.
//
// .SECTION Widget State
// When creating the widget, the mode is Start. In START mode, the
// LeftButtonPressEvent defines the bone's head. The widget then goes
// to DEFINE mode. In DEFINE mode, a LeftButtonPressEvent defines the
// bone's tail. The widget then goes to REST mode.
// In rest mode, the user can move, resize, translate the bone, its head or
// its tail. The rest transform is automatically updated to represent new
// coordinates such as the world coordinates Y will be aligned with the bone's
// directionnal vector.
// In POSE mode, the bone will keep its size. The user can only rotate
// the tail of the bone in the camera plane when interacting.
// The pose transform is updated such as the world coordinate Y will be
// aligned with the bone directionnal vector when transformed by the product
// of the pose transform and the rest transform.
// The START mode and the DEFINED mode should not be set manually by the user.
// To resume, the bone state-machine is the following:
// START ---> Define ---> REST <<--->> POSE
//
// .SECTION Forward kinematic
// The user can also specify a rotation and translation to the bone, called
// WorldToParentRotation and WorldToParentTranslation. In that case, the
// bone will be represented in the given coordinate system.
//
// In REST mode, the bone will automatically compute the following
// rotations and translation:
//  - World -> Bone.
//  - Parent -> Bone.
//
// In POSE mode, the bone will recompute its position to always have the same
// position in the Parent coordinate system (forward kinematic).
//
// .SECTION Representation
// The representation associated are the vtkBoneWidgetRepresentation (a line),
// the vtkCylinderBoneRepresentation (a cylinder around a line) and the
// vtkDoubleConeRepresentation (2 cones with theirs base glued together around
// a line).
//
// @sa vtkArmatureWidget vtkBoneWidgetRepresentation
// @sa vtkCylinderBoneRepresentation vtkDoubleConeRepresentation

// Bender includes
#include "vtkBenderWidgetsExport.h"
#include "vtkQuaternion.h"
#include "vtkQuaternionSetGet.h"

// VTK includes
#include <vtkAbstractWidget.h>
#include <vtkCommand.h>
#include <vtkSmartPointer.h>
#include <vtkStdString.h>

class vtkAxesActor;
class vtkBoneRepresentation;
class vtkBoneWidgetCallback;
class vtkHandleWidget;
class vtkLineRepresentation;
class vtkLineWidget2;
class vtkPolyDataMapper;
class vtkTransform;

class VTK_BENDER_WIDGETS_EXPORT vtkBoneWidget : public vtkAbstractWidget
{
public:
  // Description:
  // Instantiate this class.
  static vtkBoneWidget *New();

  // Description:
  // Standard methods for a VTK class.
  vtkTypeMacro(vtkBoneWidget, vtkAbstractWidget);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Set/Get a bone's name. Default is an empty string.
  vtkSetMacro(Name, vtkStdString);
  vtkGetMacro(Name, vtkStdString);

  // Description:
  // The method for activiating and deactiviating this widget. This method
  // must be overridden because it is a composite widget and does more than
  // its superclasses' vtkAbstractWidget::SetEnabled() method.
  virtual void SetEnabled(int);

  // Description:
  // Set the bone representation.
  // @sa CreateDefaultRepresentation() GetBoneRepresentation()
  // @sa vtkBoneWidgetRepresentation vtkCylinderBoneRepresentation
  // @sa vtkDoubleConeRepresentation
  void SetRepresentation(vtkBoneRepresentation* r);

  // Description:
  // Return the representation as a vtkBoneRepresentation.
  // @sa vtkBoneWidgetRepresentation vtkCylinderBoneRepresentation
  // @sa vtkDoubleConeRepresentation
  vtkBoneRepresentation* GetBoneRepresentation();

  // Description:
  // Create the default widget representation(vtkBoneRepresentation)
  // if none is set.
  // @sa vtkBoneWidgetRepresentation SetRepresentation()
  virtual void CreateDefaultRepresentation();

  // Description:
  // Methods to change the whether the widget responds to interaction.
  // Overridden to pass the state to component widgets.
  virtual void SetProcessEvents(int process);

  // Description:
  // RestChangedEvent: Fired in rest mode when a point or a transform
  // was updated.
  // PoseChangedEvent: Fired in pose mode when a point or a transform
  // was updated.
  // SelectedStateChangedEvent: Fired whenever the bone is selected
  // or unselected.
  //BTX
  enum BoneWidgetEventType {RestChangedEvent = vtkCommand::UserEvent + 1,
                            PoseChangedEvent,
                            SelectedStateChangedEvent
                           };
  //ETX

  // Description:
  // The modes work following this state-machine diagram:
  // PlaceHead -> PlaceTail -> Rest <-> Pose
  //
  // PlaceHead Mode: Define the first point when clicked.
  //  Goes then to PlaceTail mode.
  // PlaceTail Mode: Define the second point when clicked.
  //  Goes then to rest mode.
  // Rest Mode: The bone can be moved and rescaled. If the parent transform is
  //  changed, it will simply recompute its local points in the
  //  new coordinate system.
  // Pose Mode:  The bone can only be rotated. (NO rescaling). If the parent
  //  transform is changed, it will recompute its global points to
  //  regarding the new coordinate system.
  //BTX
  enum WidgetStateType {PlaceHead=0,PlaceTail,Rest,Pose};
  //ETX

  // Description:
  // Set/Get the widget state.
  // Rest Mode:  The bone can be moved and rescaled.
  // Pose Mode:  The bone can only be rotated.
  // Start mode and define mode can only be achieved when creating a
  // new bone and interactively placing its points.
  // @sa WidgetStateType()
  vtkGetMacro(WidgetState, int);
  void SetWidgetState(int state);
  void SetWidgetStateToRest();
  void SetWidgetStateToPose();

  // Description
  // Set/Get the roll angle, in radians (0.0 by default). The roll is
  // a rotation of axis the bone directionnal vector and amplitude Roll
  // that is applied to the rest transform everytime it's recomputed.
  // It will make the bone rotate around himself.
  vtkGetMacro(Roll, double);
  vtkSetMacro(Roll, double);

  // Description:
  // Get the current head/tail world position.
  void GetCurrentWorldHead(double head[3]) const;
  const double* GetCurrentWorldHead() const;
  void GetCurrentWorldTail(double tail[3]) const;
  const double* GetCurrentWorldTail() const;

  // Description:
  // Rest mode Set methods.
  // Those methods set the world to parent REST transformation.
  // When linking multiple bones together, those methods should be
  // updated everytime the bone parent has changed.
  // NOTE: When updating both the rotation and the translation,
  // the method SetWorldToParentRestRotationAndTranslation() should
  // be used in order to prevent the bone to update and fire the
  // RestChangedEvent twice.
  // @sa RestChangedEvent
  void SetWorldToParentRestRotationAndTranslation(double quat[4],
                                                  double translate[3]);
  void SetWorldToParentRestRotation(double quat[4]);
  void SetWorldToParentRestTranslation(double translate[3]);

  // Description:
  // Rest mode get methods.
  // Access methods to all the rotation and translations from the rest mode.
  vtkGetQuaterniondMacro(WorldToParentRestRotation, double);
  vtkGetVector3Macro(WorldToParentRestTranslation, double);

  vtkGetQuaterniondMacro(ParentToBoneRestRotation, double);
  vtkGetVector3Macro(ParentToBoneRestTranslation, double);

  vtkGetQuaterniondMacro(WorldToBoneRestRotation, double);
  vtkGetVector3Macro(WorldToBoneHeadRestTranslation, double);
  vtkGetVector3Macro(WorldToBoneTailRestTranslation, double);

  // Description:
  // Rest mode methods to quickly create transforms.
  vtkSmartPointer<vtkTransform> CreateWorldToBoneRestTransform() const;
  vtkSmartPointer<vtkTransform> CreateWorldToBoneRestRotation() const;

  vtkSmartPointer<vtkTransform> CreateWorldToParentRestTransform() const;
  vtkSmartPointer<vtkTransform> CreateWorldToParentRestRotation() const;

  vtkSmartPointer<vtkTransform> CreateParentToBoneRestTransform() const;
  vtkSmartPointer<vtkTransform> CreateParentToBoneRestRotation() const;

  // Description:
  // Pose mode Set methods.
  // Those methods set the world to parent POSE transformation.
  // When linking multiple bones together, those methods should be
  // updated everytime the bone parent has changed.
  // NOTE: When updating both the rotation and the translation,
  // the method SetWorldToParentPoseRotationAndTranslation() should
  // be used in order to prevent the bone to update and fire the
  // PoseChangedEvent twice.
  // @sa PoseChangedEvent
  void SetWorldToParentPoseRotationAndTranslation(double quat[4],
                                                  double translate[3]);
  void SetWorldToParentPoseRotation(double quat[4]);
  void SetWorldToParentPoseTranslation(double translate[3]);

  // Description:
  // Pose mode get methods.
  // Access methods to all the rotation and translations from the pose mode.
  vtkGetQuaterniondMacro(WorldToParentPoseRotation, double);
  vtkGetVector3Macro(WorldToParentPoseTranslation, double);

  vtkGetQuaterniondMacro(ParentToBonePoseRotation, double);
  vtkGetVector3Macro(ParentToBonePoseTranslation, double);

  vtkGetQuaterniondMacro(WorldToBonePoseRotation, double);
  vtkGetVector3Macro(WorldToBoneHeadPoseTranslation, double);
  vtkGetVector3Macro(WorldToBoneTailPoseTranslation, double);

  vtkGetQuaterniondMacro(RestToPoseRotation, double);
  //void SetRestToPoseRotation(double rotation[4]); //\TO DO ?

  // Description:
  // Pose mode methods to quickly create transforms.
  vtkSmartPointer<vtkTransform> CreateWorldToBonePoseTransform() const;
  vtkSmartPointer<vtkTransform> CreateWorldToBonePoseRotation() const;

  vtkSmartPointer<vtkTransform> CreateWorldToParentPoseTransform() const;
  vtkSmartPointer<vtkTransform> CreateWorldToParentPoseRotation() const;

  vtkSmartPointer<vtkTransform> CreateParentToBonePoseTransform() const;
  vtkSmartPointer<vtkTransform> CreateParentToBonePoseRotation() const;

  // Description:
  // Set the head/tail rest world position.
  // These methods assume that the bone is in rest mode.
  // @sa GetWorldHeadRest() GetWorldTailRest()
  void SetWorldHeadAndTailRest(double head[3], double tail[3]);
  void SetWorldHeadRest(double x, double y, double z);
  void SetWorldHeadRest(double head[3]);
  void SetWorldTailRest(double x, double y, double z);
  void SetWorldTailRest(double tail[3]);

  // Description:
  // Set the head/tail rest display position.
  // These methods assume that the bone is in rest mode
  // and has a representation.
  void SetDisplayHeadRestPosition(double x, double y);
  void SetDisplayHeadRestPosition(double head[3]);
  void SetDisplayTailRestPosition(double x, double y);
  void SetDisplayTailRestPosition(double tail[3]);

  // Description:
  // Get rest and pose mode world positions.
  // @sa SetWorldHeadRest() SetWorldTailRest()
  vtkGetVector3Macro(WorldHeadRest, double);
  vtkGetVector3Macro(WorldTailRest, double);
  vtkGetVector3Macro(WorldHeadPose, double);
  vtkGetVector3Macro(WorldTailPose, double);

  // Description:
  // Set the head/tail rest local position.
  // These methods assume that the bone is in rest mode.
  // @sa GetLocalHeadRest() GetLocalTailRest()
  void SetLocalHeadAndTailRest(double head[3], double tail[3]);
  void SetLocalHeadRest(double x, double y, double z);
  void SetLocalHeadRest(double head[3]);
  void SetLocalTailRest(double x, double y, double z);
  void SetLocalTailRest(double tail[3]);

  // Description:
  // Get rest and pose mode local positions.
  // @sa SetLocalRestHead() SetLocalRestTail()
  vtkGetVector3Macro(LocalHeadRest, double);
  vtkGetVector3Macro(LocalTailRest, double);
  vtkGetVector3Macro(LocalHeadPose, double);
  vtkGetVector3Macro(LocalTailPose, double);

  // Description:
  // Get the distance between the current head and tail.
  double GetLength();

  // Description
  // Set/get if the debug axes are visible or not.
  // Nothing:  Show nothing
  // ShowRestTransform: The debug axes will output the rest transform axes.
  // ShowPoseTransform: The debug axes will output the pose transform axes.
  // @sa GetAxesActor().
  vtkGetMacro(ShowAxes, int);
  void SetShowAxes(int show);

  // Description:
  // Set the get the axes size factor. The axes final size is the bone's
  // length multiplied b this factor.
  // The values is between 0 and 1, default is 0.4.
  void SetAxesSize(double size);
  vtkGetMacro(AxesSize, double);

  // Description:
  // Hidden:  Hide the axes.
  // ShowRestTransform: The debug axes will output the RestTransform axes.
  // ShowPoseTransform: The debug axes will output the pose transform axes.
  //BTX
  enum ShowAxesType
    {
    Hidden = 0,
    ShowRestTransform,
    ShowPoseTransform,
    };
  //ETX

  // Description:
  // Get the Axes actor. This is meant for the user to modify the rendering
  // properties of the actor. The other properties must be left unchanged.
  // @sa GetShowAxes() SetShowAxes()
  vtkAxesActor* GetAxesActor();

  // Description:
  // Get the parenthood line representation. This is meant for the user to modify
  // the rendering properties of the line.
  // The other properties must be left unchanged.
  // @sa GetShowParenthood() SetShowParenthood()
  vtkLineRepresentation* GetParenthoodRepresentation();

  // Description:
  // Rotation methods to move the tail. Those methods can be used in
  // rest or pose mode. Angle is in radians.
  void RotateTailX(double angle);
  void RotateTailY(double angle);
  void RotateTailZ(double angle);
  void RotateTailWXYZ(double angle, double x, double y, double z);
  void RotateTailWXYZ(double angle, double axis[3]); //TO CHECK !

  // Description
  // Show/Hide the link between a child's head an its parent origin.
  // This link will of course be invisible if the bone is attached
  // to its parent. True by default.
  vtkGetMacro(ShowParenthood, int);
  void SetShowParenthood (int parenthood);

  // Description:
  // Reset the pose mode to the same positions and transformations than
  // the rest positions and transformations.
  void ResetPoseToRest();

  // Description:
  // Get the selection state of the widget.
  vtkGetMacro(BoneSelected, int);

  // Description:
  // The differents selection state of the widget.
  //BTX
  enum WidgetSelectedState
    {
    NotSelected = 0,
    HeadSelected,
    TailSelected,
    LineSelected
    };
  //ETX

  // Description:
  // Deep copy another bone's properties.
  // Note: The representation type (if any) is left unchanged.
  void DeepCopy(vtkBoneWidget* other);

protected:
  vtkBoneWidget();
  ~vtkBoneWidget();

  // Bone Name
  vtkStdString Name;

  // The different states of the widget.
  int WidgetState;
  int BoneSelected;

  // Callback interface to capture events when placing the widget.
  static void StartSelectAction(vtkAbstractWidget*);
  static void MoveAction(vtkAbstractWidget*);
  static void EndSelectAction(vtkAbstractWidget*);

  //BTX
  friend class vtkBoneWidgetCallback;
  //ETX

  // The positioning handle widgets.
  vtkHandleWidget* HeadWidget;
  vtkHandleWidget* TailWidget;
  vtkHandleWidget* LineWidget;

  // Methods invoked when the handles at the
  // end points of the widget are manipulated.
  virtual void StartInteraction();
  virtual void EndInteraction();

  // Return the selected state from interaction state.
  // It converts the vtkBoneRepresentation enum into the widget selected state
  // enum.
  int GetSelectedStateFromInteractionState(int interactionState);
  vtkAbstractWidget* GetHandleFromInteractionState(int state);
  vtkWidgetRepresentation* GetSelectedRepresentation();
  void SetWidgetStateInternal(int state);

  // Warning, the new tail shouldn't change the length of the bone.
  void SetWorldTailPose(double tail[3]);

  // Bone widget essentials
  // World positions:
  // - Rest:
  double WorldHeadRest[3];
  double WorldTailRest[3];
  // - Pose:
  double WorldHeadPose[3];
  double WorldTailPose[3];
  // Local Positions:
  // - Rest:
  double LocalHeadRest[3];
  double LocalTailRest[3];
  // - Pose
  double LocalHeadPose[3];
  double LocalTailPose[3];

  // Roll Angle:
  double Roll; // in radians

  // Note to myself:
  // World to bone rotation - three solutions:
  // - Recomputed everytime it's asked (quaternion product and normalization)
  // - Recomputed everytime the parent or the bone orientation changes and
  //   stored.
  // So far: stored for faster access.
  //
  // Alternative solution. I clearly don't use Parent to bone much.
  // (It's even recomputed from WorldToBone and WorldToParent).
  // Maybe I could just get rid of it ?

  // Transforms:
  // - Rest Transforms:
  //   * Parent To Bone:
  vtkQuaterniond ParentToBoneRestRotation;
  double ParentToBoneRestTranslation[3]; // <-> LocalRestHead.
  //   * World To Parent:
  vtkQuaterniond WorldToParentRestRotation;// Given.
  double WorldToParentRestTranslation[3];// Given.
  //   * World To Bone:
  vtkQuaterniond WorldToBoneRestRotation;
  double WorldToBoneHeadRestTranslation[3];
  double WorldToBoneTailRestTranslation[3];

  // - Pose Transforms:
  //   * Rest To Pose (<-> Rotate Tail around Head):
  //double BoneRestToPoseRotation[4];
  //   * Parent To Bone:
  vtkQuaterniond ParentToBonePoseRotation;
  double ParentToBonePoseTranslation[3]; // LocalPoseHead.
  //   * World To Parent:
  vtkQuaterniond WorldToParentPoseRotation;
  double WorldToParentPoseTranslation[3];
  //    * World To Bone:
  //       WorldToBoneRestRotation * RestToPoseRotation.
  vtkQuaterniond WorldToBonePoseRotation;
  double WorldToBoneHeadPoseTranslation[3];
  double WorldToBoneTailPoseTranslation[3];

  // - Pose Interaction transform:
  //   * To hold the BoneRestToPoseRotation during interaction.
  vtkQuaterniond StartPoseRotation;

  // - Pose To Rest Transform:
  //   * Holds the rotation between the local rest tail
  //     and the local pose tail.
  vtkQuaterniond RestToPoseRotation;

  // To hold the old world position while interacting.
  // This enables to recompute the RestToPose rotation from scratch
  // while interacting.
  double InteractionWorldHeadPose[3];
  double InteractionWorldTailPose[3];

  // Update visibility of the widget and associated actors such as axes,
  // parenthood, etc...
  // @sa UpdateShowAxes(), UpdateParenthoodLinkVisibility()
  void UpdateVisibility();

  // Axes variables:
  // For an easier debug and understanding.
  int ShowAxes;
  vtkAxesActor* AxesActor;
  double AxesSize;

  // Helper methods to change the axes orientation and origin
  // with respect to the ShowAxes variable and the bone's transforms.
  // Note: RebuildParentageLink() is yo be called when the visibility of
  // the axes may be subject  to change. This will call RebuildAxes().
  void RebuildAxes();
  void UpdateShowAxes();

  // Parentage line
  int ShowParenthood;
  vtkLineWidget2* ParenthoodLink;

  // Helper methods to change the Parenthood line origin
  // with respect to the ParentTranslation.
  // Note: UpdateParenthoodLinkVisibility() should be called
  // when the visibility of the line may be subject 
  // to change. This will call RebuildParenthoodLink().
  void RebuildParenthoodLink();
  void UpdateParenthoodLinkVisibility();
  // Create the line representation and set the proper variables.
  void InstantiateParenthoodLink();

  // Tansforms Essentials functions
  // Rest Mode
  //  - Rotations:
  void RebuildParentToBoneRestRotation();
  void RebuildWorldToBoneRestRotation();

  //  - Translations:
  void RebuildParentToBoneRestTranslation();
  void RebuildWorldToBoneRestTranslations(); // Compute both WorldToBoneHead
                                             // and WorldToBoneTail.

  // Pose Mode:
  //  - Rotations:
  void RebuildParentToBonePoseRotation();
  void RebuildWorldToBonePoseRotationInteraction();
  void RebuildWorldToBonePoseRotationFromParent();

  //  - Translations:
  void RebuildParentToBonePoseTranslation();
  void RebuildWorldToBonePoseTranslations(); // Compute both WorldToBoneHead
                                             // and WorldToBoneTail.

  // Recompute local points from the world positions.
  void RebuildLocalRestPoints();
  void RebuildLocalPosePoints();
  void RebuildLocalTailPose();

  // Helpers function that gather calls to Rebuilds functions.
  void UpdateRestMode();
  void UpdatePoseMode();

  // Update the representation (if any) to world points.
  void UpdateRepresentation();
  void UpdateDisplay();

  // Compute new world points from the pose transforms and local points.
  void UpdateWorldRestPositions();
  void UpdateWorldPosePositions();

  // Update the pose interaction variable
  void UpdatePoseIntercationsVariables();

  // Update the transformation between the local rest tail and local pose head.
  void UpdateRestToPoseRotation();

  //
  // Computation functions:
  //
  // Compute the rotation between a frame from which the Y axis is given
  // to the bone frame (where Y is the current bone line) and store it in
  // the given quaternion.
  // Axis is the reference frame Y axis (vector 3D) and newOrientation the
  // rotation computed (quaternion).
  // Also applies a rotation aroud the new Y axis and of amplitude Roll.
  // (If roll not zero)
  vtkQuaterniond ComputeRotationFromReferenceAxis(const double* axis);

  // Return if the camera axis exist (there is a renderer and a camera) and if
  // it is orthogonal to the given vector.
  bool ShouldUseCameraAxisForPoseTransform(
    const double* vec1, const double* vec2);

  // Rotate the current tail on itself by a rotation of angle and axis.
  void RotateTail(double angle, double axis[3], double newTail[3]);

  // Rebuild the pose mode's worlds points depending on the Rest points.
  void RebuildPoseFromRest();

  // Init pose mode with rest values.
  bool ShouldInitializePoseMode;
  void InitializePoseMode();

  // Selects and highlight the widget representation
  void SetWidgetSelectedState(int selectionState);

private:
  vtkBoneWidget(const vtkBoneWidget&);  //Not implemented
  void operator=(const vtkBoneWidget&);  //Not implemented
};

#endif
