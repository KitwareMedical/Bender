/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Johan Andruejol, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

/// Qt includes
#include <QDebug>
#include <QVariant>

// CTK includes
#include <ctkFlowLayout.h>
#include <ctkPopupWidget.h>
#include <ctkVTKRenderView.h>

/// Bender includes
#include <vtkArmatureWidget.h>
#include <vtkBVHReader.h>

/// Armature includes
#include "qSlicerArmaturesIOOptionsWidget.h"
#include "ui_qSlicerArmaturesIOOptionsWidget.h"

/// Slicer includes
#include <qMRMLUtils.h>
#include <qSlicerApplication.h>
#include <qSlicerIOOptions_p.h>
#include <qSlicerWidget.h>

// VTK includes
#include <vtkCamera.h>
#include <vtkRenderer.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_Volumes
class qSlicerArmaturesIOOptionsWidgetPrivate
  : public qSlicerIOOptionsPrivate
  , public Ui_qSlicerArmaturesIOOptionsWidget
{
  Q_DECLARE_PUBLIC(qSlicerArmaturesIOOptionsWidget);

protected:
  qSlicerArmaturesIOOptionsWidget* const q_ptr;

public:
  typedef Ui_qSlicerArmaturesIOOptionsWidget Superclass;

  qSlicerArmaturesIOOptionsWidgetPrivate(
    qSlicerArmaturesIOOptionsWidget& object);
  ~qSlicerArmaturesIOOptionsWidgetPrivate();

  void setupUi(qSlicerWidget* slicerArmaturesIOOptionsWidget);

  vtkBVHReader* Reader;
  ctkVTKRenderView* RenderView;

  ctkPopupWidget* Popup;

  void createRendering(QString filename);
  void deleteRendering();
};

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidgetPrivate
::qSlicerArmaturesIOOptionsWidgetPrivate(
  qSlicerArmaturesIOOptionsWidget& object)
  : q_ptr(&object)
{
  this->Reader = 0;
  this->RenderView = 0;
}

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidgetPrivate::
~qSlicerArmaturesIOOptionsWidgetPrivate()
{
  this->deleteRendering();
  if (this->RenderView)
    {
    delete this->RenderView;
    }
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidgetPrivate
::setupUi(qSlicerWidget* slicerArmaturesIOOptionsWidget)
{
  Q_Q(qSlicerArmaturesIOOptionsWidget);
  this->Superclass::setupUi(slicerArmaturesIOOptionsWidget);

  this->Popup = new ctkPopupWidget(this->FrameSliderWidget);
  QHBoxLayout* popupLayout = new QHBoxLayout(this->Popup);
  this->RenderView = new ctkVTKRenderView(this->Popup);
  this->RenderView->setFixedSize(QSize( 200, 200));
  this->RenderView->scheduleRender();
  popupLayout->addWidget(this->RenderView);
  int left, top, right, bottom;
  popupLayout->getContentsMargins(&left, &top, &right, &bottom);
  this->Popup->setMaximumSize(QSize( 200 + left + right , 200 + top + bottom));
  this->Popup->setAutoShow(true);
  this->Popup->setAutoHide(true);
  this->Popup->setVerticalDirection(ctkBasePopupWidget::BottomToTop);
  this->Popup->setAlignment(Qt::AlignJustify | Qt::AlignTop);

  q->connect(this->FrameSelectionEnabledCheckBox, SIGNAL(stateChanged(int)),
    q, SLOT(enableFrameChange(int)));
  q->connect(this->FrameSliderWidget, SIGNAL(valueChanged(double)),
    q, SLOT(updateProperties()));
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidgetPrivate::createRendering(QString filename)
{
  if (this->Reader)
    {
    return;
    }

  this->Reader = vtkBVHReader::New();
  this->Reader->SetFileName(filename.toLatin1());
  this->Reader->Update();

  if (!this->Reader->GetArmature())
    {
    this->Reader->Delete();
    this->Reader = 0;
    return;
    }

  vtkArmatureWidget* armature = this->Reader->GetArmature();
  if (armature)
    {
    armature->SetInteractor(this->RenderView->interactor());
    armature->SetCurrentRenderer(this->RenderView->renderer());
    armature->On();
    armature->SetProcessEvents(false);
    }
  this->FrameSliderWidget->setMaximum(this->Reader->GetNumberOfFrames() - 1);
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidgetPrivate::deleteRendering()
{
  if (this->Reader)
    {
    this->FrameSliderWidget->setValue(0);
    this->FrameSliderWidget->setMaximum(0);

    if (this->Reader->GetArmature())
      {
      this->Reader->GetArmature()->SetEnabled(0);
      }
    this->Reader->Delete();
    this->Reader = 0;
    }
}

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidget
::qSlicerArmaturesIOOptionsWidget(QWidget* parentWidget)
  : qSlicerIOOptionsWidget(
    new qSlicerArmaturesIOOptionsWidgetPrivate(*this), parentWidget)
{
  Q_D(qSlicerArmaturesIOOptionsWidget);
  d->setupUi(this);
  ctkFlowLayout::replaceLayout(this);
}

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidget::~qSlicerArmaturesIOOptionsWidget()
{
}

//------------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget::setFileName(const QString& fileName)
{
  Q_D(qSlicerArmaturesIOOptionsWidget);

  // Save selectFrame value before enableFrameChange modifies it.
  const bool selectFrame = d->Properties.contains("selectFrame") ?
    d->Properties["selectFrame"].toBool() :
    d->FrameSelectionEnabledCheckBox->isChecked();

  // Disable rendering
  d->FrameSelectionEnabledCheckBox->setChecked(false);

  this->Superclass::setFileName(fileName);

  const bool isBVH = fileName.lastIndexOf(".bvh") != -1;
  this->setEnabled(isBVH);
  if (isBVH)
    {
    d->FrameSelectionEnabledCheckBox->setChecked(selectFrame);
    }

}

//------------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget
::setFileNames(const QStringList& fileNames)
{
  this->setFileName(fileNames.size() > 0 ? fileNames.front() : "");
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget::updateProperties()
{
  Q_D(qSlicerArmaturesIOOptionsWidget);
  d->Properties["frame"] = d->FrameSelectionEnabledCheckBox->isChecked() ?
    static_cast<unsigned int>( d->FrameSliderWidget->value()) : 0;
  d->Properties["selectFrame"] = d->FrameSelectionEnabledCheckBox->isChecked();
  this->updateToolTip();
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget::enableFrameChange(int enable)
{
  Q_D(qSlicerArmaturesIOOptionsWidget);

  d->FrameSliderWidget->setEnabled(enable);
  if (!enable)
    {
    d->deleteRendering();
    }
  else
    {
    d->createRendering(d->Properties["fileName"].toString());
    }
  this->updateProperties();
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget::updateToolTip()
{
  Q_D(qSlicerArmaturesIOOptionsWidget);
  if (!this->isEnabled() || !d->Reader)
    {
    return;
    }

  d->Reader->SetFrame(d->FrameSliderWidget->value());
  d->Reader->Update();
  if (d->Reader->GetArmature())
    {
    d->RenderView->renderer()->ResetCamera(
      d->Reader->GetArmature()->GetPolyData()->GetBounds());
    }
  d->RenderView->scheduleRender();
}
