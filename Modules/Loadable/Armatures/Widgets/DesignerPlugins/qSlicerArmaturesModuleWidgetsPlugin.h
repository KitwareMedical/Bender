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

#ifndef __qSlicerArmaturesModuleWidgetsPlugin_h
#define __qSlicerArmaturesModuleWidgetsPlugin_h

// Qt includes
#include <QDesignerCustomWidgetCollectionInterface>

// Armaturess includes
#include "qMRMLArmaturesAnimationWidgetPlugin.h"

// \class Group the plugins in one library
class Q_SLICER_MODULE_ARMATURES_WIDGETS_PLUGINS_EXPORT qSlicerArmaturesModuleWidgetsPlugin
  : public QObject
  , public QDesignerCustomWidgetCollectionInterface
{
  Q_OBJECT
  Q_INTERFACES(QDesignerCustomWidgetCollectionInterface);

public:
  QList<QDesignerCustomWidgetInterface*> customWidgets() const
    {
    QList<QDesignerCustomWidgetInterface *> plugins;
    plugins << new qMRMLArmaturesAnimationWidgetPlugin;
    return plugins;
    }
};

#endif
