/**************************************************************************/
/*  GodotEditor.kt                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

package org.redotengine.editor

/**
 * Primary window of the Godot Editor.
 *
 * This is the implementation of the editor used when running on HorizonOS devices.
 */
open class GodotEditor : BaseGodotEditor() {

	override fun getExcludedPermissions(): MutableSet<String> {
		val excludedPermissions = super.getExcludedPermissions().apply {
			// The AVATAR_CAMERA and HEADSET_CAMERA permissions are requested when `CameraFeed.feed_is_active`
			// is enabled.
			add("horizonos.permission.AVATAR_CAMERA")
			add("horizonos.permission.HEADSET_CAMERA")
		}
		return excludedPermissions
	}

	override fun getXRRuntimePermissions(): MutableSet<String> {
		val xrRuntimePermissions = super.getXRRuntimePermissions()
		xrRuntimePermissions.add("com.oculus.permission.USE_SCENE")
		xrRuntimePermissions.add("horizonos.permission.USE_SCENE")
		return xrRuntimePermissions
	}
}
