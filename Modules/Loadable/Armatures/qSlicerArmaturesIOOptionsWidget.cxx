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

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

/// Qt includes
#include <QBuffer>
#include <QDebug>
#include <QImage>
#include <QLabel>
#include <QVariant>

// CTK includes
#include <ctkFlowLayout.h>
#include <ctkPopupWidget.h>

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
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWindowToImageFilter.h>

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
  vtkRenderer* Renderer;
  vtkRenderWindow* RenderWindow;
  vtkRenderWindowInteractor* RenderWindowInteractor;
  vtkWindowToImageFilter* WindowToImageFilter;

  ctkPopupWidget* Popup;
  QLabel* DisplayLabel;

  void createRendering(QString filename);
  void deleteRendering();
};

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidgetPrivate
::qSlicerArmaturesIOOptionsWidgetPrivate(
  qSlicerArmaturesIOOptionsWidget& object)
  : q_ptr(&object)
{
  Q_Q(qSlicerArmaturesIOOptionsWidget);

  this->Reader = 0;
  this->Renderer = 0;
  this->RenderWindow = 0;
  this->RenderWindowInteractor = 0;
  this->WindowToImageFilter = 0;

}

//-----------------------------------------------------------------------------
qSlicerArmaturesIOOptionsWidgetPrivate::
~qSlicerArmaturesIOOptionsWidgetPrivate()
{
  this->deleteRendering();
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidgetPrivate
::setupUi(qSlicerWidget* slicerArmaturesIOOptionsWidget)
{
  Q_Q(qSlicerArmaturesIOOptionsWidget);
  this->Superclass::setupUi(slicerArmaturesIOOptionsWidget);

  this->Popup = new ctkPopupWidget(this->FrameSliderWidget);
  QHBoxLayout* popupLayout = new QHBoxLayout(this->Popup);
  this->DisplayLabel = new QLabel(this->Popup);
  this->DisplayLabel->setScaledContents(true);
  popupLayout->addWidget(this->DisplayLabel);
  this->Popup->setAutoShow(true);
  this->Popup->setAutoHide(true);

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

  Q_Q(qSlicerArmaturesIOOptionsWidget);

  this->Reader = vtkBVHReader::New();
  this->Renderer = vtkRenderer::New();
  this->RenderWindow = vtkRenderWindow::New();
  this->RenderWindowInteractor = vtkRenderWindowInteractor::New();

  this->RenderWindow->AddRenderer(this->Renderer);
  this->RenderWindowInteractor->SetRenderWindow(this->RenderWindow);

  this->Renderer->GetActiveCamera()->SetPosition(0.0, 1.0, 0.0);

  this->WindowToImageFilter = vtkWindowToImageFilter::New();
  this->WindowToImageFilter->SetInput(this->RenderWindow);

  this->Reader->SetFileName(filename.toLatin1());
  this->Reader->Update();

  if (!this->Reader->GetArmature())
    {
    this->deleteRendering();
    }
  else
    {
    this->Reader->GetArmature()->SetInteractor(this->RenderWindowInteractor);
    this->Reader->GetArmature()->SetCurrentRenderer(this->Renderer);

    this->RenderWindow->SetOffScreenRendering(1);
    this->RenderWindow->SetSize(
      this->FrameSliderWidget->width(), this->FrameSliderWidget->width());
    this->RenderWindow->Render();
    this->RenderWindowInteractor->Initialize();
    this->RenderWindow->Render();
    this->Reader->GetArmature()->On();

    this->FrameSliderWidget->setMaximum(this->Reader->GetNumberOfFrames() - 1);
    }
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidgetPrivate::deleteRendering()
{
  Q_Q(qSlicerArmaturesIOOptionsWidget);

  if (this->Reader)
    {
    this->DisplayLabel->setText("");
    this->FrameSliderWidget->setValue(0);
    this->FrameSliderWidget->setMaximum(0);

    this->Reader->Delete();
    this->Reader = 0;
    this->Renderer->Delete();
    this->Renderer = 0;
    this->RenderWindow->Delete();
    this->RenderWindow = 0;
    this->RenderWindowInteractor->Delete();
    this->RenderWindowInteractor = 0;
    this->WindowToImageFilter->Delete();
    this->WindowToImageFilter = 0;
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

  if (d->Properties.contains("filename")
    && fileName != d->Properties["filename"])
    {
    this->enableFrameChange(false);
    }

  this->Superclass::setFileName(fileName);
  this->setEnabled(fileName.lastIndexOf(".bvh") != -1);
}

//------------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget
::setFileNames(const QStringList& fileNames)
{
  Q_D(qSlicerArmaturesIOOptionsWidget);
  this->setFileName(fileNames.size() > 0 ? fileNames.front() : "");
}

//-----------------------------------------------------------------------------
void qSlicerArmaturesIOOptionsWidget::updateProperties()
{
  Q_D(qSlicerArmaturesIOOptionsWidget);
  d->Properties["frame"] =
    static_cast<unsigned int>(d->FrameSliderWidget->value());
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
  if (d->Reader->GetArmature())
    {
    d->Renderer->ResetCamera(
      d->Reader->GetArmature()->GetPolyData()->GetBounds());
    }

  d->WindowToImageFilter->Modified(); // See vtkWindowToImageFilter
  d->WindowToImageFilter->Update();

  QImage image;
  vtkImageData* frame = d->WindowToImageFilter->GetOutput();
  qMRMLUtils::vtkImageDataToQImage(frame, image);
  d->DisplayLabel->setPixmap(QPixmap::fromImage(image));
}
