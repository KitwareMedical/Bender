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

// Qt includes

// SlicerApp includes
#include "qSlicerAppAboutDialog.h"
#include "qSlicerApplication.h"
#include "ui_qSlicerAppAboutDialog.h"

// Slicer includes
#include <vtkSlicerVersionConfigure.h>

//-----------------------------------------------------------------------------
class qSlicerAppAboutDialogPrivate: public Ui_qSlicerAppAboutDialog
{
public:
};

//-----------------------------------------------------------------------------
// qSlicerAppAboutDialogPrivate methods


//-----------------------------------------------------------------------------
// qSlicerAppAboutDialog methods
qSlicerAppAboutDialog::qSlicerAppAboutDialog(QWidget* parentWidget)
 :QDialog(parentWidget)
  , d_ptr(new qSlicerAppAboutDialogPrivate)
{
  Q_D(qSlicerAppAboutDialog);
  d->setupUi(this);

  qSlicerApplication* slicer = qSlicerApplication::application();
  d->CreditsTextBrowser->setFontPointSize(25);

  // [Ninja]

  // Logos
  d->AppLabel->setPixmap(QPixmap(QString::fromUtf8(":/Logo.png")));

  // Credits
  d->CreditsTextBrowser->append(slicer->applicationName());
  d->CreditsTextBrowser->setFontPointSize(11);
  d->CreditsTextBrowser->append("");
  /*d->CreditsTextBrowser->append(
    slicer->applicationVersion()+ " "
    + "r" + slicer->repositoryRevision());*/
  d->CreditsTextBrowser->append( "Version: Bender " + slicer->applicationVersion());
  d->CreditsTextBrowser->append("Revision: " + slicer->repositoryRevision());
  d->CreditsTextBrowser->append("");
  d->CreditsTextBrowser->append("Authors: <i>Johan Andruejol (Kitware)"
                                ", Julien Finet (Kitware)"
                                ", Leo Liu (Kitware)"
                                "</i>");
  d->CreditsTextBrowser->append("");
  d->CreditsTextBrowser->append(QString("Based on: <a href=\"http://www.slicer.org/\">Slicer %1</a>").arg(Slicer_VERSION_FULL));
  d->CreditsTextBrowser->append("");
  //d->CreditsTextBrowser->insertHtml(slicer->acknowledgment());
  d->CreditsTextBrowser->insertHtml(slicer->libraries());
  //d->SlicerLinksTable->setIndexWidget(QModelIndex(), new QTextBrowser);
  //d->SlicerLinksTextBrowser->insertHtml(slicer->copyrights());
  d->CreditsTextBrowser->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
  // [/Ninja]
}

//-----------------------------------------------------------------------------
qSlicerAppAboutDialog::~qSlicerAppAboutDialog()
{
}
