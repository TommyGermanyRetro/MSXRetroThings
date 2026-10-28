<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()>
Partial Class frmMain
    Inherits System.Windows.Forms.Form

    ' Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()>
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    ' Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    ' NOTE: The following procedure is required by the Windows Form Designer
    ' It can be modified using the Windows Form Designer.
    ' Do not modify it using the code editor.
    <System.Diagnostics.DebuggerStepThrough()>
    Private Sub InitializeComponent()
        Me.components = New System.ComponentModel.Container()
        Dim resources As System.ComponentModel.ComponentResourceManager = New System.ComponentModel.ComponentResourceManager(GetType(frmMain))
        Me.SerialPort = New System.IO.Ports.SerialPort(Me.components)
        Me.MenuStrip = New System.Windows.Forms.MenuStrip()
        Me.frmMain_ToolStripMenuItem_Verbindung = New System.Windows.Forms.ToolStripMenuItem()
        Me.frmMain_ToolStripMenuItem_Einstellen = New System.Windows.Forms.ToolStripMenuItem()
        Me.frmMain_ToolStripMenuItem_Verbinden = New System.Windows.Forms.ToolStripMenuItem()
        Me.frmMain_ToolStripMenuItem_Trennen = New System.Windows.Forms.ToolStripMenuItem()
        Me.StatusStrip = New System.Windows.Forms.StatusStrip()
        Me.ToolStripCOM = New System.Windows.Forms.ToolStripStatusLabel()
        Me.pbVDP = New System.Windows.Forms.PictureBox()
        Me.VDP_Timer_In = New System.Windows.Forms.Timer(Me.components)
        Me.pbBACK = New System.Windows.Forms.PictureBox()
        Me.rtbStatus = New System.Windows.Forms.RichTextBox()
        Me.MenuStrip.SuspendLayout()
        Me.StatusStrip.SuspendLayout()
        CType(Me.pbVDP, System.ComponentModel.ISupportInitialize).BeginInit()
        CType(Me.pbBACK, System.ComponentModel.ISupportInitialize).BeginInit()
        Me.SuspendLayout()
        '
        'SerialPort
        '
        Me.SerialPort.PortName = "COM8"
        '
        'MenuStrip
        '
        Me.MenuStrip.ImageScalingSize = New System.Drawing.Size(24, 24)
        Me.MenuStrip.Items.AddRange(New System.Windows.Forms.ToolStripItem() {Me.frmMain_ToolStripMenuItem_Verbindung})
        Me.MenuStrip.Location = New System.Drawing.Point(0, 0)
        Me.MenuStrip.Name = "MenuStrip"
        Me.MenuStrip.Padding = New System.Windows.Forms.Padding(4, 1, 0, 1)
        Me.MenuStrip.Size = New System.Drawing.Size(1584, 24)
        Me.MenuStrip.TabIndex = 0
        Me.MenuStrip.Text = "MenuStrip"
        '
        'frmMain_ToolStripMenuItem_Verbindung
        '
        Me.frmMain_ToolStripMenuItem_Verbindung.DropDownItems.AddRange(New System.Windows.Forms.ToolStripItem() {Me.frmMain_ToolStripMenuItem_Einstellen, Me.frmMain_ToolStripMenuItem_Verbinden, Me.frmMain_ToolStripMenuItem_Trennen})
        Me.frmMain_ToolStripMenuItem_Verbindung.Name = "frmMain_ToolStripMenuItem_Verbindung"
        Me.frmMain_ToolStripMenuItem_Verbindung.Size = New System.Drawing.Size(80, 22)
        Me.frmMain_ToolStripMenuItem_Verbindung.Text = "Verbindung"
        '
        'frmMain_ToolStripMenuItem_Einstellen
        '
        Me.frmMain_ToolStripMenuItem_Einstellen.Name = "frmMain_ToolStripMenuItem_Einstellen"
        Me.frmMain_ToolStripMenuItem_Einstellen.Size = New System.Drawing.Size(127, 22)
        Me.frmMain_ToolStripMenuItem_Einstellen.Text = "Einstellen"
        '
        'frmMain_ToolStripMenuItem_Verbinden
        '
        Me.frmMain_ToolStripMenuItem_Verbinden.Name = "frmMain_ToolStripMenuItem_Verbinden"
        Me.frmMain_ToolStripMenuItem_Verbinden.Size = New System.Drawing.Size(127, 22)
        Me.frmMain_ToolStripMenuItem_Verbinden.Text = "Verbinden"
        '
        'frmMain_ToolStripMenuItem_Trennen
        '
        Me.frmMain_ToolStripMenuItem_Trennen.Name = "frmMain_ToolStripMenuItem_Trennen"
        Me.frmMain_ToolStripMenuItem_Trennen.Size = New System.Drawing.Size(127, 22)
        Me.frmMain_ToolStripMenuItem_Trennen.Text = "Trennen"
        '
        'StatusStrip
        '
        Me.StatusStrip.AutoSize = False
        Me.StatusStrip.ImageScalingSize = New System.Drawing.Size(24, 24)
        Me.StatusStrip.Items.AddRange(New System.Windows.Forms.ToolStripItem() {Me.ToolStripCOM})
        Me.StatusStrip.Location = New System.Drawing.Point(0, 838)
        Me.StatusStrip.Name = "StatusStrip"
        Me.StatusStrip.Size = New System.Drawing.Size(1584, 23)
        Me.StatusStrip.TabIndex = 3
        Me.StatusStrip.Text = "StatusStrip"
        '
        'ToolStripCOM
        '
        Me.ToolStripCOM.BorderSides = System.Windows.Forms.ToolStripStatusLabelBorderSides.Right
        Me.ToolStripCOM.Name = "ToolStripCOM"
        Me.ToolStripCOM.Size = New System.Drawing.Size(53, 18)
        Me.ToolStripCOM.Text = "RS232: -"
        '
        'pbVDP
        '
        Me.pbVDP.Location = New System.Drawing.Point(21, 48)
        Me.pbVDP.Name = "pbVDP"
        Me.pbVDP.Size = New System.Drawing.Size(1024, 768)
        Me.pbVDP.TabIndex = 0
        Me.pbVDP.TabStop = False
        '
        'VDP_Timer_In
        '
        Me.VDP_Timer_In.Interval = 1
        '
        'pbBACK
        '
        Me.pbBACK.Location = New System.Drawing.Point(12, 37)
        Me.pbBACK.Name = "pbBACK"
        Me.pbBACK.Size = New System.Drawing.Size(1044, 788)
        Me.pbBACK.TabIndex = 25
        Me.pbBACK.TabStop = False
        '
        'rtbStatus
        '
        Me.rtbStatus.BackColor = System.Drawing.SystemColors.WindowText
        Me.rtbStatus.BorderStyle = System.Windows.Forms.BorderStyle.None
        Me.rtbStatus.Font = New System.Drawing.Font("Courier New", 10.0!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
        Me.rtbStatus.ForeColor = System.Drawing.SystemColors.Window
        Me.rtbStatus.Location = New System.Drawing.Point(1075, 37)
        Me.rtbStatus.Margin = New System.Windows.Forms.Padding(20)
        Me.rtbStatus.Name = "rtbStatus"
        Me.rtbStatus.ReadOnly = True
        Me.rtbStatus.Size = New System.Drawing.Size(498, 788)
        Me.rtbStatus.TabIndex = 26
        Me.rtbStatus.Text = ""
        '
        'frmMain
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.AutoScroll = True
        Me.AutoSize = True
        Me.ClientSize = New System.Drawing.Size(1584, 861)
        Me.Controls.Add(Me.rtbStatus)
        Me.Controls.Add(Me.pbVDP)
        Me.Controls.Add(Me.StatusStrip)
        Me.Controls.Add(Me.MenuStrip)
        Me.Controls.Add(Me.pbBACK)
        Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle
        Me.Icon = CType(resources.GetObject("$this.Icon"), System.Drawing.Icon)
        Me.MainMenuStrip = Me.MenuStrip
        Me.MaximumSize = New System.Drawing.Size(1600, 900)
        Me.Name = "frmMain"
        Me.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide
        Me.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen
        Me.Text = "MSX-1 TMS9918A-Emulator via USB"
        Me.MenuStrip.ResumeLayout(False)
        Me.MenuStrip.PerformLayout()
        Me.StatusStrip.ResumeLayout(False)
        Me.StatusStrip.PerformLayout()
        CType(Me.pbVDP, System.ComponentModel.ISupportInitialize).EndInit()
        CType(Me.pbBACK, System.ComponentModel.ISupportInitialize).EndInit()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub

    Friend WithEvents SerialPort As IO.Ports.SerialPort
    Friend WithEvents MenuStrip As MenuStrip
    Friend WithEvents frmMain_ToolStripMenuItem_Verbindung As ToolStripMenuItem
    Friend WithEvents frmMain_ToolStripMenuItem_Einstellen As ToolStripMenuItem
    Friend WithEvents frmMain_ToolStripMenuItem_Verbinden As ToolStripMenuItem
    Friend WithEvents frmMain_ToolStripMenuItem_Trennen As ToolStripMenuItem
    Friend WithEvents StatusStrip As StatusStrip
    Friend WithEvents ToolStripCOM As ToolStripStatusLabel
    Friend WithEvents FrameTimer As Timer
    Friend WithEvents pbVDP As PictureBox
    Friend WithEvents VDP_Timer_In As Timer
    Friend WithEvents pbBACK As PictureBox
    Friend WithEvents rtbStatus As RichTextBox
End Class
