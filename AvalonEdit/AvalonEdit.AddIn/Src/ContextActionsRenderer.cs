﻿// Copyright (c) AlphaSierraPapa for the SharpDevelop Team (for details please see \doc\copyright.txt)
// This code is distributed under the GNU LGPL (for details please see \doc\license.txt)

using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Input;
using System.Windows.Threading;
using Revsoft.Core;
using Revsoft.SharpDevelop;
using Revsoft.SharpDevelop.Editor;
using Revsoft.SharpDevelop.Editor.AvalonEdit;
using Revsoft.SharpDevelop.Gui;
using Revsoft.SharpDevelop.Refactoring;

namespace Revsoft.Wabbitcode.AvalonEditExtension
{
	/// <summary>
	/// Renders Popup with context actions on the left side of the current line in the editor.
	/// </summary>
	public sealed class ContextActionsRenderer : IDisposable
	{
		readonly CodeEditorView editorView;
		ITextEditor Editor { get { return this.editorView.Adapter; } }
		/// <summary>
		/// This popup is reused (closed and opened again).
		/// </summary>
		ContextActionsBulbPopup popup = new ContextActionsBulbPopup();
		
		/// <summary>
		/// Delays the available actions resolution so that it does not get called too often when user holds an arrow.
		/// </summary>
		DispatcherTimer delayMoveTimer;
		const int delayMoveMilliseconds = 500;
		
		public bool IsEnabled
		{
			get {
				string fileName = this.Editor.FileName;
				if (String.IsNullOrEmpty(fileName))
					return false;
				return fileName.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)
					|| fileName.EndsWith(".vb", StringComparison.OrdinalIgnoreCase);
			}
		}
		
		public ContextActionsRenderer(CodeEditorView editor)
		{
			if (editor == null)
				throw new ArgumentNullException("editor");
			this.editorView = editor;
			
			this.editorView.TextArea.Caret.PositionChanged += CaretPositionChanged;
			
			this.editorView.KeyDown += new KeyEventHandler(ContextActionsRenderer_KeyDown);
			
			editor.TextArea.TextView.ScrollOffsetChanged += ScrollChanged;
			this.delayMoveTimer = new DispatcherTimer() { Interval = TimeSpan.FromMilliseconds(delayMoveMilliseconds) };
			this.delayMoveTimer.Stop();
			this.delayMoveTimer.Tick += TimerMoveTick;
			WorkbenchSingleton.Workbench.ActiveViewContentChanged += WorkbenchSingleton_Workbench_ActiveViewContentChanged;
		}
		
		public void Dispose()
		{
			ClosePopup();
			WorkbenchSingleton.Workbench.ActiveViewContentChanged -= WorkbenchSingleton_Workbench_ActiveViewContentChanged;
			delayMoveTimer.Stop();
		}
		
		void ContextActionsRenderer_KeyDown(object sender, KeyEventArgs e)
		{
			if (this.popup == null)
				return;
			if (e.Key == Key.T && Keyboard.Modifiers == ModifierKeys.Control)
			{
				if (popup.ViewModel != null && popup.ViewModel.Actions != null && popup.ViewModel.Actions.Count > 0) {
					popup.IsDropdownOpen = true;
					popup.Focus();
				} else {
					// Popup is not shown but user explicitely requests it
					var popupVM = BuildPopupViewModel(this.Editor);
					popupVM.LoadHiddenActions();
					if (popupVM.HiddenActions.Count == 0)
						return;
					this.popup.ViewModel = popupVM;
					this.popup.IsDropdownOpen = true;
					this.popup.IsHiddenActionsExpanded = true;
					this.popup.OpenAtLineStart(this.Editor);
					this.popup.Focus();
				}
			}
		}

		void ScrollChanged(object sender, EventArgs e)
		{
			ClosePopup();
		}

		void TimerMoveTick(object sender, EventArgs e)
		{
			this.delayMoveTimer.Stop();
			if (!IsEnabled)
				return;
			ClosePopup();
			
			ContextActionsBulbViewModel popupVM = BuildPopupViewModel(this.Editor);
			//availableActionsVM.Title =
			//availableActionsVM.Image =
			if (popupVM.Actions.Count == 0)
				return;
			this.popup.ViewModel = popupVM;
			this.popup.OpenAtLineStart(this.Editor);
		}

		ContextActionsBulbViewModel BuildPopupViewModel(ITextEditor editor)
		{
			var actionsProvider = ContextActionsService.Instance.GetAvailableActions(editor);
			return new ContextActionsBulbViewModel(actionsProvider);
		}

		void CaretPositionChanged(object sender, EventArgs e)
		{
			if (this.popup.IsOpen)
			{
				ClosePopup();
			}
			this.delayMoveTimer.Stop();
			this.delayMoveTimer.Start();
		}
		
		void ClosePopup()
		{
			this.popup.Close();
			this.popup.IsDropdownOpen = false;
			this.popup.IsHiddenActionsExpanded = false;
			this.popup.ViewModel = null;
		}
		void WorkbenchSingleton_Workbench_ActiveViewContentChanged(object sender, EventArgs e)
		{
			// open the popup again if in current file
			IViewContent activeViewContent = WorkbenchSingleton.Workbench.ActiveViewContent;
			if (activeViewContent != null && activeViewContent.PrimaryFileName == this.Editor.FileName)
				CaretPositionChanged(this, EventArgs.Empty);
			else // otherwise close popup
				ClosePopup();
		}
	}
}
