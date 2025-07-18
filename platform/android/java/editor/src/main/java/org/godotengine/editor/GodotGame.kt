/**************************************************************************/
/*  GodotGame.kt                                                          */
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

import android.app.PictureInPictureParams
import android.content.pm.PackageManager
import android.graphics.Rect
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.View
import androidx.annotation.CallSuper
import androidx.core.view.isVisible
import org.redotengine.editor.embed.GameMenuFragment
import org.redotengine.godot.GodotLib
import org.redotengine.godot.editor.utils.GameMenuUtils
import org.redotengine.godot.utils.ProcessPhoenix
import org.redotengine.godot.utils.isHorizonOSDevice
import org.redotengine.godot.utils.isNativeXRDevice
import org.redotengine.godot.xr.HYBRID_APP_PANEL_FEATURE
import org.redotengine.godot.xr.XRMode
import org.redotengine.godot.xr.isHybridAppEnabled

/**
 * Drives the 'run project' window of the Godot Editor.
 */
open class GodotGame : BaseGodotGame() {

	companion object {
		private val TAG = GodotGame::class.java.simpleName
	}

	private val gameViewSourceRectHint = Rect()
	private val expandGameMenuButton: View? by lazy { findViewById(R.id.game_menu_expand_button) }

	override fun onCreate(savedInstanceState: Bundle?) {
		gameMenuState.clear()
		intent.getBundleExtra(EXTRA_GAME_MENU_STATE)?.let {
			gameMenuState.putAll(it)
		}
		gameMenuState.putBoolean(EXTRA_IS_GAME_EMBEDDED, isGameEmbedded())
		gameMenuState.putBoolean(EXTRA_IS_GAME_RUNNING, true)

		super.onCreate(savedInstanceState)

		gameMenuContainer?.isVisible = shouldShowGameMenuBar()
		expandGameMenuButton?.apply{
			isVisible = shouldShowGameMenuBar() && isMenuBarCollapsable()
			setOnClickListener {
				gameMenuFragment?.expandGameMenu()
			}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			val gameView = findViewById<View>(R.id.godot_fragment_container)
			gameView?.addOnLayoutChangeListener { v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom ->
				gameView.getGlobalVisibleRect(gameViewSourceRectHint)
			}
		}
	}

	override fun getCommandLine(): MutableList<String> {
		val updatedArgs = super.getCommandLine()
		if (!updatedArgs.contains(XRMode.REGULAR.cmdLineArg)) {
			updatedArgs.add(XRMode.REGULAR.cmdLineArg)
		}
		if (!updatedArgs.contains(XR_MODE_ARG)) {
			updatedArgs.add(XR_MODE_ARG)
			updatedArgs.add("off")
		}
		return updatedArgs
	}

	override fun enterPiPMode() {
		if (hasPiPSystemFeature()) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
				val builder = PictureInPictureParams.Builder().setSourceRectHint(gameViewSourceRectHint)
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
					builder.setSeamlessResizeEnabled(false)
				}
				setPictureInPictureParams(builder.build())
			}

			Log.v(TAG, "Entering PiP mode")
			enterPictureInPictureMode()
		}
	}

	/**
	 * Returns true the if the device supports picture-in-picture (PiP).
	 */
	protected fun hasPiPSystemFeature(): Boolean {
		return packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
	}

	override fun shouldShowGameMenuBar(): Boolean {
		return intent.getBooleanExtra(
			EXTRA_EDITOR_HINT,
			false
		) && gameMenuContainer != null
	}

	override fun onPictureInPictureModeChanged(isInPictureInPictureMode: Boolean) {
		super.onPictureInPictureModeChanged(isInPictureInPictureMode)
		Log.v(TAG, "onPictureInPictureModeChanged: $isInPictureInPictureMode")

		// Hide the game menu fragment when in PiP.
		gameMenuContainer?.isVisible = !isInPictureInPictureMode
	}

	override fun onStop() {
		super.onStop()

		if (isInPictureInPictureMode && !isFinishing) {
			// We get in this state when PiP is closed, so we terminate the activity.
			finish()
		}
	}

	override fun getGodotAppLayout() = R.layout.godot_game_layout

	override fun getEditorWindowInfo() = RUN_GAME_INFO

	override fun getEditorGameEmbedMode() = GameMenuUtils.GameEmbedMode.DISABLED

	override fun overrideOrientationRequest() = false

	override fun suspendGame(suspended: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SUSPEND)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, suspended)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun dispatchNextFrame() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_NEXT_FRAME)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun toggleSelectionVisibility(enabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SELECTION_VISIBLE)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, enabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun overrideCamera(enabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_CAMERA_OVERRIDE)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, enabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun selectRuntimeNode(nodeType: GameMenuFragment.GameMenuListener.NodeType) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_NODE_TYPE)
			putSerializable(KEY_GAME_MENU_ACTION_PARAM1, nodeType)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun selectRuntimeNodeSelectMode(selectMode: GameMenuFragment.GameMenuListener.SelectMode) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SELECT_MODE)
			putSerializable(KEY_GAME_MENU_ACTION_PARAM1, selectMode)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun reset2DCamera() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_RESET_CAMERA_2D_POSITION)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun reset3DCamera() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_RESET_CAMERA_3D_POSITION)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun manipulateCamera(mode: GameMenuFragment.GameMenuListener.CameraMode) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_CAMERA_MANIPULATE_MODE)
			putSerializable(KEY_GAME_MENU_ACTION_PARAM1, mode)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun muteAudio(enabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_DEBUG_MUTE_AUDIO)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, enabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun embedGameOnPlay(embedded: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_EMBED_GAME_ON_PLAY)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, embedded)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	protected open fun isGameEmbedded() = false

	override fun isGameEmbeddingSupported() = !isNativeXRDevice(applicationContext)

	override fun isMinimizedButtonEnabled() = isTaskRoot && !isNativeXRDevice(applicationContext)

	override fun isCloseButtonEnabled() = !isHorizonOSDevice(applicationContext)

	override fun isPiPButtonEnabled() = hasPiPSystemFeature()

	override fun isMenuBarCollapsable() = true

	override fun minimizeGameWindow() {
		moveTaskToBack(false)
	}

	override fun closeGameWindow() {
		ProcessPhoenix.forceQuit(this)
	}

	override fun onGameMenuCollapsed(collapsed: Boolean) {
		expandGameMenuButton?.isVisible = shouldShowGameMenuBar() && isMenuBarCollapsable() && collapsed
	}

	@CallSuper
	override fun supportsFeature(featureTag: String): Boolean {
		if (HYBRID_APP_PANEL_FEATURE == featureTag) {
			// Check if openxr is enabled
			if (!GodotLib.getGlobal("xr/openxr/enabled").toBoolean()) {
				return false
			}

			// Check if hybrid is enabled
			return isHybridAppEnabled()
		}

		return super.supportsFeature(featureTag)
	}

}
