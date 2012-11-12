#=========================================================================
#
#  Program: Bender
#
#  Copyright (c) Kitware Inc.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0.txt
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#=========================================================================

bl_info = {
    "name": "Export Armature",
    "author": "Julien Finet",
    "version": (0, 2),
    "blender": (2, 6, 3),
    "location": "File > Export > Armature (.arm)",
    "description": "Export Armature",
    "category": "Import-Export"}

"""
Credit to:
- io_export_psk_psa
"""


#=========================================================================
"""
USAGE

"""
#=========================================================================

import os
import time
import bpy
import mathutils
import math
import random
import operator
import sys

from struct import pack

#=========================================================================
# Custom exception class
#=========================================================================
class Error( Exception ):

  def __init__(self, message):
    self.message = message


#=========================================================================
# Verbose logging with loop truncation
#=========================================================================
def verbose( msg, iteration=-1, max_iterations=4, msg_truncated="..." ):

  if bpy.context.scene.arm_option_verbose == True:
    # limit the number of times a loop can output messages
    if iteration > max_iterations:
      return
    elif iteration == max_iterations:
      print(msg_truncated)
      return

    print(msg)
  

#=========================================================================
# Log header/separator
#=========================================================================
def header( msg, justify='LEFT', spacer='_', cols=78 ):
  
  if justify == 'LEFT':
    s = '{:{spacer}<{cols}}'.format(msg+" ", spacer=spacer, cols=cols)
  
  elif justify == 'RIGHT':
    s = '{:{spacer}>{cols}}'.format(" "+msg, spacer=spacer, cols=cols)
  
  else:
    s = '{:{spacer}^{cols}}'.format(" "+msg+" ", spacer=spacer, cols=cols)
  
  return "\n" + s + "\n"



#=========================================================================
# ARMATURE DATA STRUCTS
#=========================================================================
class FQuat:

  def __init__(self): 
    self.X = 0.0
    self.Y = 0.0
    self.Z = 0.0
    self.W = 1.0
    
  def dump(self):
    return pack('ffff', self.X, self.Y, self.Z, self.W)

  def dumpAscii(self):
    return str(self.X) + ' ' + str(self.Y) + ' ' + str(self.Z) + ' ' + str(self.W)
    
  def __cmp__(self, other):
    return cmp(self.X, other.X) \
      or cmp(self.Y, other.Y) \
      or cmp(self.Z, other.Z) \
      or cmp(self.W, other.W)
    
  def __hash__(self):
    return hash(self.X) ^ hash(self.Y) ^ hash(self.Z) ^ hash(self.W)
    
  def __str__(self):
    return "[%f,%f,%f,%f](FQuat)" % (self.X, self.Y, self.Z, self.W)


class FVector(object):

  def __init__(self, X=0.0, Y=0.0, Z=0.0):
    self.X = X
    self.Y = Y
    self.Z = Z
    
  def dump(self):
    return pack('fff', self.X, self.Y, self.Z)
    
  def dumpAscii(self):
    return str(self.X) + ' ' + str(self.Y) + ' ' + str(self.Z)
    
  def __cmp__(self, other):
    return cmp(self.X, other.X) \
      or cmp(self.Y, other.Y) \
      or cmp(self.Z, other.Z)
    
  def _key(self):
    return (type(self).__name__, self.X, self.Y, self.Z)
    
  def __hash__(self):
    return hash(self._key())
    
  def __eq__(self, other):
    if not hasattr(other, '_key'):
      return False
    return self._key() == other._key() 
    
  def dot(self, other):
    return self.X * other.X + self.Y * other.Y + self.Z * other.Z
  
  def cross(self, other):
    return FVector(self.Y * other.Z - self.Z * other.Y,
        self.Z * other.X - self.X * other.Z,
        self.X * other.Y - self.Y * other.X)
        
  def sub(self, other):
    return FVector(self.X - other.X,
      self.Y - other.Y,
      self.Z - other.Z)

# END ARMATURE DATA STRUCTS
#=========================================================================

#=========================================================================
# Helpers to create bone structs
#=========================================================================

def make_fquat( bquat ):
  quat  = FQuat()
  #flip handedness for UT = set x,y,z to negative (rotate in other direction)
  quat.X  = -bquat.x
  quat.Y  = -bquat.y
  quat.Z  = -bquat.z
  quat.W  = bquat.w
  return quat
  
def make_fquat_default( bquat ):
  quat  = FQuat()
  #print(dir(bquat))
  quat.X  = bquat.x
  quat.Y  = bquat.y
  quat.Z  = bquat.z
  quat.W  = bquat.w
  return quat

def make_vector( bquat ):
  vector  =  FVector()
  #print(dir(bquat))
  vector.X  = bquat.x
  vector.Y  = bquat.y
  vector.Z  = bquat.z
  return vector

#=========================================================================
# Collate bones that belong to the arm skeletal mesh
#=========================================================================
def parse_armature( armature ):
  
  print(header("ARMATURE", 'RIGHT'))
  verbose("Armature object: {} Armature data: {}".format(armature.name, armature.data.name))
  
  # generate a list of root bone candidates
  root_candidates = [b for b in armature.data.bones if b.parent == None]
  
  # should be a single, unambiguous result
  if len(root_candidates) == 0:
    raise Error("Cannot find root for arm bones. The root bone must use deform.")
  
  #if len(root_candidates) > 1:
  #  raise Error("Ambiguous root for arm. More than one root bone is using deform.")
  
  # prep for bone collection
  BoneUtil.static_bone_id = 0  # replaces global
  
  quat = make_fquat(armature.matrix_local.to_quaternion())
  res = '<armature '
  res += 'name=\"' + armature.name + '\"  '
  res += 'dataname=\"' + armature.data.name + '\" '
  res += 'orientation=\"' + quat.dumpAscii() + '\" '
  res += '>\n'
  # traverse bone chain
  print("{: <3} {: <48} {: <20}".format("ID", "Bone", "Status"))
  print()
  for root_bone in root_candidates:
    res += recurse_bone(root_bone, armature, 0, armature.matrix_local, ' ')
  res += '</armature>\n'
  return res


#=========================================================================
# bone        current bone
# bones        bone list
# parent_id
# parent_matrix
# indent      text indent for recursive log
#=========================================================================
def recurse_bone( bone, armature, parent_id, parent_matrix, indent="" ):
  
  head = make_vector(bone.head)
  tail = make_vector(bone.tail)
  
  # calc parented bone transform
  if bone.parent != None:
    quat    = make_fquat_default(bone.matrix.to_quaternion())
    #quat_parent  = bone.parent.matrix.to_quaternion().inverted()
    #parent_head  = quat_parent * bone.parent.head
    #parent_tail  = quat_parent * bone.parent.tail
    #translation  = (parent_tail - parent_head) + bone.head
    head = make_vector(bone.head)
    tail = make_vector(bone.tail)

  # calc root bone transform
  else:
      #translation  = parent_matrix * bone.head        # ARMATURE OBJECT Location
      #rot_matrix  = bone.matrix * parent_matrix.to_3x3()  # ARMATURE OBJECT Rotation
      #quat    = make_fquat_default(rot_matrix.to_quaternion())
      quat = make_fquat_default(bone.matrix.to_quaternion())
  
  bone_id    = BoneUtil.static_bone_id  # ALT VERS
  BoneUtil.static_bone_id += 1      # ALT VERS
  
  child_count = len(bone.children)

  print("{:<3} {:<48}".format(bone_id, indent+bone.name))
  print (bone.matrix.to_quaternion())
  print(quat.dumpAscii())
  #bone.matrix_local
  res = indent + '<bone '
  res += 'name=\"' + bone.name + '\" '
  res += 'id=\"' + str(bone_id) + '\" '
  res += 'head=\"' + head.dumpAscii() + '\" '
  res += 'tail=\"' + tail.dumpAscii() + '\" '
  res += 'orientation=\"' + quat.dumpAscii() + '\" '
  res += '>\n'

  # search matching pose bone
  poses = [x for x in armature.pose.bones if x.name == bone.name]
  if len(poses) > 0:
    pose = poses[0]
    rotation = make_fquat_default(pose.rotation_quaternion)
    res += indent + '<pose '
    res += 'rotation=\"' + rotation.dumpAscii() + '\" '
    res += '/>\n'

  # all vertex groups of the mesh (obj)...
  for mesh in armature.children:
    vertex_groups = [x for x in mesh.vertex_groups if x.name == bone.name]
    # check that the mesh has a vertex group
    if len(vertex_groups) == 0:
      raise Error("No vertex group" + bone.name + "in" + mesh.name)
      continue
    vertex_group = vertex_groups[0]
    vertices = []
    # all vertices in the mesh...
    for vertex in mesh.data.vertices:
      # all groups this vertex is a member of...
      for vgroup in vertex.groups:
        if vgroup.group == vertex_group.index:
          vertices.append(vertex.index)
    if len(vertices) > 0:
      res += indent + '<vertex_group>'
      for vertex in vertices:
        res += str(vertex) + ' '
      res += '</vertex_group>\n'

  #recursively dump child bones
  for child_bone in bone.children:
    res += recurse_bone(child_bone, armature, bone_id, parent_matrix, " " + indent)
  res += indent + '</bone>\n'
  return res

# FIXME rename? remove?
class BoneUtil:
  static_bone_id = 0 # static property to replace global

#=========================================================================
# Locate the target armature and mesh for export
# RETURNS armature, mesh
#=========================================================================
def find_armature_and_mesh():
  verbose(header("find_armature_and_mesh", 'LEFT', '<', 60))
  
  context      = bpy.context
  active_object  = context.active_object
  armature    = None
  mesh      = None
  
  # TODO:
  # this could be more intuitive
  bpy.ops.object.mode_set(mode='OBJECT')
  # try the active object
  if active_object and active_object.type == 'ARMATURE':
    armature = active_object
  
  # otherwise, try for a single armature in the scene
  else:
    all_armatures = [obj for obj in context.scene.objects if obj.type == 'ARMATURE']
    
    if len(all_armatures) == 1:
      armature = all_armatures[0]
    
    elif len(all_armatures) > 1:
      raise Error("Please select an armature in the scene")
    
    else:
      raise Error("No armatures in scene")
  
  verbose("Found armature: {}".format(armature.name))
  
  meshselected = []
  parented_meshes = [obj for obj in armature.children if obj.type == 'MESH']
  for obj in armature.children:
    #print(dir(obj))
    if obj.type == 'MESH' and obj.select == True:
      meshselected.append(obj)
  # try the active object
  if active_object and active_object.type == 'MESH' and len(meshselected) == 0:
  
    if active_object.parent == armature:
      mesh = active_object
    
    else:
      raise Error("The selected mesh is not parented to the armature")
  
  # otherwise, expect a single mesh parented to the armature (other object types are ignored)
  else:
    print("Number of meshes:",len(parented_meshes))
    print("Number of meshes (selected):",len(meshselected))
    if len(parented_meshes) == 1:
      mesh = parented_meshes[0]
      
    elif len(parented_meshes) > 1:
      if len(meshselected) >= 1:
        mesh = sortmesh(meshselected)
      else:
        raise Error("More than one mesh(s) parented to armature. Select object(s)!")
    else:
      raise Error("No mesh parented to armature")
    
  verbose("Found mesh: {}".format(mesh.name))  
  if len(armature.pose.bones) == len(mesh.vertex_groups):
    print("Armature and Mesh Vertex Groups matches Ok!")
  else:
    raise Error("Armature bones:" + str(len(armature.pose.bones)) + " Mesh Vertex Groups:" + str(len(mesh.vertex_groups)) +" doesn't match!")
  return armature, mesh

#=========================================================================
# Main
#=========================================================================
def export(filepath):
  print(header("Export", 'RIGHT'))
  t    = time.clock()
  context  = bpy.context
  
  print("Filepath: {}".format(filepath))
  
  # find armature and mesh
  # [change this to implement alternative methods; raise Error() if not found]
  arm_armature, arm_mesh = find_armature_and_mesh()
  
  # check misc conditions
  if not (arm_armature.scale.x == arm_armature.scale.y == arm_armature.scale.z == 1):
    raise Error("bad armature scale: armature object should have uniform scale of 1 (ALT-S)")
  
  if not (arm_mesh.scale.x == arm_mesh.scale.y == arm_mesh.scale.z == 1):
    raise Error("bad mesh scale: mesh object should have uniform scale of 1 (ALT-S)")
  
  if not (arm_armature.location.x == arm_armature.location.y == arm_armature.location.z == 0):
    raise Error("bad armature location: armature should be located at origin (ALT-G)")
  
  if not (arm_mesh.location.x == arm_mesh.location.y == arm_mesh.location.z == 0):
    raise Error("bad mesh location: mesh should be located at origin (ALT-G)")
  
  # step 1
  #parse_mesh(arm_mesh, arm)
  
  # step 2
  res = parse_armature(arm_armature)
  
  # write files
  print(header("Exporting", 'CENTER'))
  
  #arm_filename = filepath + '.arm'
  arm_filename = '/Users/exxos/Armature.arm'
  print("Armature data...")
  file = open(arm_filename, "w") 
  file.write(res)
  file.close() 
  print("Exported: " + arm_filename)
  print()

  print("Export completed in {:.2f} seconds".format((time.clock() - t)))

from bpy.props import *

#=========================================================================
# Operator
#=========================================================================
class Operator_ARMExport( bpy.types.Operator ):

  bl_idname  = "object.arm_export"
  bl_label  = "Export now"
  __doc__    = "Export to ARM"
  
  def execute(self, context):
    print( "\n"*8 )
    
    scene = bpy.context.scene
      
    filepath = get_dst_path()
    
    # cache settings
    restore_frame = scene.frame_current
    
    message = "Finish Export!"
    try:
      export(filepath)

    except Error as err:
      print(err.message)
      message = err.message
    
    # restore settings
    scene.frame_set(restore_frame)
        
    self.report({'ERROR'}, message)
    
    # restore settings
    scene.frame_set(restore_frame)
    
    return {'FINISHED'}

#=========================================================================
# Operator
#=========================================================================
class Operator_ToggleConsole( bpy.types.Operator ):

  bl_idname  = "object.toggle_console"
  bl_label  = "Toggle console"
  __doc__    = "Show or hide the console"
  
  #def invoke(self, context, event):
  #   bpy.ops.wm.console_toggle()
  #   return{'FINISHED'}
  def execute(self, context):
    bpy.ops.wm.console_toggle()
    return {'FINISHED'}


#=========================================================================
# Get filepath for export
#=========================================================================
def get_dst_path():
  if bpy.context.scene.arm_option_filename_src == '0':
    if bpy.context.active_object:
      path = os.path.split(bpy.data.filepath)[0] + "/" + bpy.context.active_object.name# + ".psk"
    else:
      path = os.path.split(bpy.data.filepath)[0] + "/" + "Unknown";
  else:
    path = os.path.splitext(bpy.data.filepath)[0]# + ".psk"
  return path

# fixme
from bpy.props import *


#Added by [MGVS]
bpy.types.Scene.arm_option_filename_src = EnumProperty(
    name    = "Filename",
    description = "Sets the name for the files",
    items    = [ ('0', "From object",  "Name will be taken from object name"),
            ('1', "From Blend",    "Name will be taken from .blend file name") ],
    default    = '0')
  
bpy.types.Scene.arm_option_clamp_uv = BoolProperty(
    name    = "Clamp UV",
    description  = "Clamp UV co-ordinates to [0-1]",
    default    = False)
    
bpy.types.Scene.arm_copy_merge = BoolProperty(
    name    = "merge mesh",
    description  = "Deal with unlinking the mesh to be remove while exporting the object.",
    default    = False)

bpy.types.Scene.arm_option_verbose = BoolProperty(
    name    = "Verbose",
    description  = "Verbose console output",
    default    = False)

bpy.types.Scene.arm_option_smoothing_groups = BoolProperty(
    name    = "Smooth Groups",
    description  = "Activate hard edges as smooth groups",
    default    = True)

bpy.types.Scene.arm_option_triangulate = BoolProperty(
    name    = "Triangulate Mesh",
    description  = "Convert Quads to Triangles",
    default    = False)
    

import bmesh
#=========================================================================
# User interface
#=========================================================================

def unpack_list(list_of_tuples):
    l = []
    for t in list_of_tuples:
        l.extend(t)
    return l
  
class Panel_ARMExport( bpy.types.Panel ):

  bl_label    = "ARM Export"
  bl_idname    = "OBJECT_PT_arm_tools"
  #bl_space_type  = "PROPERTIES"
  #bl_region_type  = "WINDOW"
  #bl_context    = "object"
  bl_space_type  = "VIEW_3D"
  bl_region_type  = "TOOLS"
  
  #def draw_header(self, context):
  #  layout = self.layout
    #obj = context.object
    #layout.prop(obj, "select", text="")
  
  #@classmethod
  #def poll(cls, context):
  #  return context.active_object

  def draw(self, context):
    layout = self.layout
    path = get_dst_path()

    object_name = ""
    #if context.object:
    #  object_name = context.object.name
    if context.active_object:
      object_name = context.active_object.name

    layout.prop(context.scene, "arm_option_verbose")

    row = layout.row()
    row.label(text="Active object: " + object_name)

    #layout.separator()

    layout.prop(context.scene, "arm_option_filename_src")
    row = layout.row()
    row.label(text=path)

    #layout.separator()

    layout.operator("object.arm_export")
    
    #layout.separator()

    #layout.separator()

class ExportARM(bpy.types.Operator):
        
    '''Export Armature'''
    bl_idname = "export_anim.arm" # this is important since its how bpy.ops.export.arm_anim_data is constructed
    bl_label = "Export ARM"
    __doc__ = """Select armature to be exported"""

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(
            default="*.arm",
            options={'HIDDEN'},
            )
    arm_option_verbose = bpy.types.Scene.arm_option_verbose
    arm_option_filename_src = bpy.types.Scene.arm_option_filename_src
    
    @classmethod
    def poll(cls, context):
        return context.active_object != None

    def execute(self, context):
        scene = bpy.context.scene
    
        filepath = get_dst_path()
    
    # cache settings
        restore_frame = scene.frame_current
    
        message = "Finish Export!"
        try:
            export(filepath)

        except Error as err:
            print(err.message)
            message = err.message
    
    # restore settings
        scene.frame_set(restore_frame)
        
        self.report({'WARNING', 'INFO'}, message)
        return {'FINISHED'}
        
    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}    
def menu_func(self, context):
    default_path = os.path.splitext(bpy.data.filepath)[0] + ".arm"
    self.layout.operator(ExportARM.bl_idname, text="Armature (.arm)").filepath = default_path

#=========================================================================
# Entry
#=========================================================================
def register():
  #print("REGISTER")
  bpy.utils.register_module(__name__)
  bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
  #print("UNREGISTER")
  bpy.utils.unregister_module(__name__)
  bpy.types.INFO_MT_file_export.remove(menu_func)
  
if __name__ == "__main__":
  #print("\n"*4)
  print(header("Export Armature 0.1", 'CENTER'))
  register()
  
