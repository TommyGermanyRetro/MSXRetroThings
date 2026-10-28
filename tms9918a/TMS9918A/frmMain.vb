Imports System.IO
Imports System.ComponentModel
Imports System.Threading

Public Class frmMain

    Public WithEvents VDP As New clVDP
    Public WithEvents COM As New clCOM
    Public VIDEO As New clVIDEO

    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    '+++ MAIN                                                                                           +++
    '+++ Functions for the form                                                                         +++
    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    Private Sub FrmMain_Load(sender As Object, e As EventArgs) Handles MyBase.Load

        ' System menu with the language selection

        frmMain_ToolStripMenuItem_System.DropDownItems.AddRange({frmMain_ToolStripMenuItem_VideoRecord, frmMain_ToolStripMenuItem_VideoStop,
                                                                 frmMain_ToolStripMenuItem_PAL, frmMain_ToolStripMenuItem_NTSC,
                                                                 New ToolStripSeparator(),
                                                                 frmMain_ToolStripMenuItem_English, frmMain_ToolStripMenuItem_Deutsch})
        MenuStrip.Items.Add(frmMain_ToolStripMenuItem_System)
        Language_Apply()
        Video_Menu()

        frmMain_ToolStripMenuItem_Trennen.Enabled = False

        If COM.PortName <> "-" Then
            frmMain_ToolStripMenuItem_Verbinden.Enabled = True
        Else
            frmMain_ToolStripMenuItem_Verbinden.Enabled = False
        End If

        ' Show the border

        pbBACK.Visible = True
        pbBACK.BackColor = VDP.CHIPCOLORTABLE(VDP.COLOR_BLACK)

        ' Show the black plane

        pbVDP.Visible = True
        pbVDP.Image = VDP.IMAGE
        VDP.PRESENT()

        rtbStatus.Clear()
        rtbStatus.BackColor = VDP.CHIPCOLORTABLE(VDP.COLOR_GRAY)
        rtbStatus.ForeColor = VDP.CHIPCOLORTABLE(VDP.COLOR_BLACK)

    End Sub

    Private Sub FrmMain_Closing(sender As Object, e As CancelEventArgs) Handles Me.Closing

        COM_Schliessen()
        Video_Stop()

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_Verbinden_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_Verbinden.Click

        COM_Oeffnen()
        VDP_Timer_In.Start()

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_Trennen_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_Trennen.Click

        VDP_Timer_In.Stop()
        COM_Schliessen()

    End Sub

    ' Language of the user interface, saved together with the COM settings

    Private ReadOnly frmMain_ToolStripMenuItem_System As New ToolStripMenuItem("System")
    Private WithEvents frmMain_ToolStripMenuItem_English As New ToolStripMenuItem("English")
    Private WithEvents frmMain_ToolStripMenuItem_Deutsch As New ToolStripMenuItem("Deutsch")

    Private Sub FrmMain_ToolStripMenuItem_English_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_English.Click

        UiText.Language = "en"
        COM.Save()
        Language_Apply()

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_Deutsch_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_Deutsch.Click

        UiText.Language = "de"
        COM.Save()
        Language_Apply()

    End Sub

    Private Sub Language_Apply()

        Me.Text = UI("MSX-1 TMS9918A emulator via USB", "MSX-1 TMS9918A-Emulator via USB")
        frmMain_ToolStripMenuItem_Verbindung.Text = UI("Connection", "Verbindung")
        frmMain_ToolStripMenuItem_Einstellen.Text = UI("Settings", "Einstellen")
        frmMain_ToolStripMenuItem_Verbinden.Text = UI("Connect", "Verbinden")
        frmMain_ToolStripMenuItem_Trennen.Text = UI("Disconnect", "Trennen")
        frmMain_ToolStripMenuItem_VideoRecord.Text = UI("Record video...", "Video aufnehmen...")
        frmMain_ToolStripMenuItem_VideoStop.Text = UI("Stop video", "Video beenden")
        frmMain_ToolStripMenuItem_PAL.Text = "Video PAL (50 Hz)"
        frmMain_ToolStripMenuItem_NTSC.Text = "Video NTSC (60 Hz)"
        frmMain_ToolStripMenuItem_English.Checked = UiText.Language <> "de"
        frmMain_ToolStripMenuItem_Deutsch.Checked = UiText.Language = "de"

        If Not Synced AndAlso COM.Client.IsOpen Then rtbStatus.Text = vbCrLf & UI(" Waiting for the RESET message of the MSX ...", " Warte auf RESET-Nachricht des MSX ...")

    End Sub

    '******************************************************************************************************
    '*** MAIN - Video recording                                                                         ***
    '******************************************************************************************************

    ' Records the picture of every field into an MP4 file (shared VIDEOREC library) until stopped

    Private WithEvents frmMain_ToolStripMenuItem_VideoRecord As New ToolStripMenuItem()
    Private WithEvents frmMain_ToolStripMenuItem_VideoStop As New ToolStripMenuItem()

    Private Sub FrmMain_ToolStripMenuItem_VideoRecord_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_VideoRecord.Click

        If VIDEO.Recording OrElse Not COM.Client.IsOpen Then Exit Sub

        Using Dialog As New SaveFileDialog()

            Dialog.Title = UI("Record video", "Video aufnehmen")
            Dialog.Filter = UI("MP4 video (*.mp4)|*.mp4|All files (*.*)|*.*", "MP4-Video (*.mp4)|*.mp4|Alle Dateien (*.*)|*.*")
            Dialog.DefaultExt = "mp4"
            Dialog.InitialDirectory = Video_Folder()
            Dialog.FileName = "TMS9918A_" & DateTime.Now.ToString("yyyyMMdd_HHmmss") & ".mp4"

            If Dialog.ShowDialog(Me) <> DialogResult.OK Then Exit Sub

            ' The frame rate follows the video standard chosen in the menu

            Dim ErrorText As String = VIDEO.Start(Dialog.FileName, clVIDEO.STANDARDTICKS)

            If ErrorText <> "" Then
                MsgBox(UI("The video recording could not be started.", "Die Videoaufnahme konnte nicht gestartet werden.") & vbCrLf & vbCrLf & ErrorText,
                       vbOKOnly Or vbCritical, UI("Record video", "Video aufnehmen"))
            Else
                ' The video starts with a RESET of the MSX
                MSX_Reset()
            End If

        End Using

        Video_Menu()

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_VideoStop_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_VideoStop.Click

        Video_Stop()

    End Sub

    ' Finishes the MP4 file; reports when writing failed

    Private Sub Video_Stop()

        Dim ErrorText As String = VIDEO.Stop()

        If ErrorText <> "" Then
            MsgBox(UI("The video could not be written completely.", "Das Video konnte nicht vollständig geschrieben werden.") & vbCrLf & vbCrLf & ErrorText,
                   vbOKOnly Or vbCritical, UI("Record video", "Video aufnehmen"))
        End If

        Video_Menu()

    End Sub

    Private Sub Video_Menu()

        frmMain_ToolStripMenuItem_VideoRecord.Enabled = Not VIDEO.Recording AndAlso COM.Client.IsOpen
        frmMain_ToolStripMenuItem_VideoStop.Enabled = VIDEO.Recording
        frmMain_ToolStripMenuItem_PAL.Enabled = Not VIDEO.Recording
        frmMain_ToolStripMenuItem_NTSC.Enabled = Not VIDEO.Recording
        frmMain_ToolStripMenuItem_PAL.Checked = Not clVIDEO.NTSC
        frmMain_ToolStripMenuItem_NTSC.Checked = clVIDEO.NTSC
        ToolStripCOM.Text = If(COM.Client.IsOpen, COM.Description, "COM: -") & Video_Status()

    End Sub

    ' Video standard of the recording, saved together with the COM settings

    Private WithEvents frmMain_ToolStripMenuItem_PAL As New ToolStripMenuItem()
    Private WithEvents frmMain_ToolStripMenuItem_NTSC As New ToolStripMenuItem()

    Private Sub FrmMain_ToolStripMenuItem_PAL_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_PAL.Click

        clVIDEO.NTSC = False
        COM.Save()
        Video_Menu()

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_NTSC_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_NTSC.Click

        clVIDEO.NTSC = True
        COM.Save()
        Video_Menu()

    End Sub

    Private Function Video_Status() As String

        If Not VIDEO.Recording Then Return ""

        Dim Seconds As Integer = CInt(Math.Floor(VIDEO.Seconds))
        Return " | REC " & (Seconds \ 60) & ":" & (Seconds Mod 60).ToString("00")

    End Function

    ' Folder for videos: "2100 - MSX - MEDIEN\VIDEO" in the development tree, otherwise
    ' Documents\TMS9918A\VIDEO, created when missing

    Private Function Video_Folder() As String

        Try

            Dim Folder As DirectoryInfo = New DirectoryInfo(Application.StartupPath)

            While Folder IsNot Nothing
                Dim Media As String = Path.Combine(Folder.FullName, "2100 - MSX - MEDIEN")
                If Directory.Exists(Media) Then
                    Directory.CreateDirectory(Path.Combine(Media, "VIDEO"))
                    Return Path.Combine(Media, "VIDEO")
                End If
                Folder = Folder.Parent
            End While

            Dim User As String = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "TMS9918A", "VIDEO")
            Directory.CreateDirectory(User)
            Return User

        Catch ex As Exception

            Return ""

        End Try

    End Function

    ' One finished frame of the VDP: into the recording, COUNT times for skipped frames

    Private Sub VDP_Field(ByVal COUNT As Long) Handles VDP.VDP_Field

        If VIDEO.FieldTicks = 0 OrElse Not frmMain_ToolStripMenuItem_VideoStop.Enabled Then Exit Sub

        ' Writing failed: the recording ends here

        If Not VIDEO.Recording Then
            Video_Stop()
            Exit Sub
        End If

        VIDEO.AddFields(VDP, COUNT)

    End Sub

    Private Sub FrmMain_ToolStripMenuItem_Einstellen_Click(sender As Object, e As EventArgs) Handles frmMain_ToolStripMenuItem_Einstellen.Click

        FrmInit.ShowDialog()

    End Sub


    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    '+++ VDP                                                                                            +++
    '+++ Functions for the video processor and the interface to the board                               +++
    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    '******************************************************************************************************
    '*** VDP - Output the screen                                                                        ***
    '******************************************************************************************************

    Private Sub VDP_Out(ByVal PLANE As Bitmap) Handles VDP.VDP_Out

        If pbVDP.Image IsNot PLANE Then pbVDP.Image = PLANE
        pbVDP.Invalidate()

        pbBACK.BackColor = VDP.CHIPCOLORTABLE(VDP.COLORBACK)

    End Sub

    '******************************************************************************************************
    '*** VDP - Measurements and status display                                                          ***
    '******************************************************************************************************

    ' Measurement interval and counter values at the start of the interval

    Private MessWatch As Stopwatch = Stopwatch.StartNew()
    Private MessFrames As Long = 0
    Private MessBytes As Long = 0
    Private MessMessages As Long = 0

    ' Rates of the last interval and peak values since connecting

    Private Fps As Double = 0
    Private BytesPerSec As Double = 0
    Private MessagesPerSec As Double = 0
    Private BytesPerSecMax As Double = 0
    Private MessagesPerSecMax As Double = 0
    Private BacklogMax As Integer = 0
    Private RingFillMax As Integer = 0

    ' Label of a status line, padded to the value column

    Private Shared Function LBL(ByVal EN As String, ByVal DE As String) As String
        Return (" " & UI(EN, DE)).PadRight(24)
    End Function

    Private Sub Messung_Zuruecksetzen()

        MessWatch.Restart()
        MessFrames = 0
        MessBytes = 0
        MessMessages = 0
        Fps = 0
        BytesPerSec = 0
        MessagesPerSec = 0
        BytesPerSecMax = 0
        MessagesPerSecMax = 0
        BacklogMax = 0
        RingFillMax = 0

    End Sub

    Private Sub Messung_Auswerten()

        Dim Backlog As Integer = COM.Backlog
        Dim RingFill As Integer = COM.RingFill

        If Backlog > BacklogMax Then BacklogMax = Backlog
        If RingFill > RingFillMax Then RingFillMax = RingFill

        If MessWatch.ElapsedMilliseconds < 1000 Then Exit Sub

        ' Compute the rates over the elapsed interval

        Dim Seconds As Double = MessWatch.ElapsedMilliseconds / 1000.0
        Dim Frames As Long = VDP.FRAMES
        Dim Bytes As Long = COM.RxBytes
        Dim Messages As Long = COM.RxMessages

        ' After a RESET the frame counter restarts

        If Frames < MessFrames Then MessFrames = 0

        Fps = (Frames - MessFrames) / Seconds
        BytesPerSec = (Bytes - MessBytes) / Seconds
        MessagesPerSec = (Messages - MessMessages) / Seconds

        If BytesPerSec > BytesPerSecMax Then BytesPerSecMax = BytesPerSec
        If MessagesPerSec > MessagesPerSecMax Then MessagesPerSecMax = MessagesPerSec

        MessFrames = Frames
        MessBytes = Bytes
        MessMessages = Messages
        MessWatch.Restart()

        ' Update the status display

        ToolStripCOM.Text = COM.Description & Video_Status()

        rtbStatus.Text = vbCrLf &
                         LBL("Sync:", "Sync:") & If(Synced, "OK", UI("waiting for RESET", "warte auf RESET")) & vbCrLf &
                         LBL("Bytes/s:", "Bytes/s:") & BytesPerSec.ToString("N0") & " (max " & BytesPerSecMax.ToString("N0") & ")" & vbCrLf &
                         LBL("Messages/s:", "Nachrichten/s:") & MessagesPerSec.ToString("N0") & " (max " & MessagesPerSecMax.ToString("N0") & ")" & vbCrLf &
                         LBL("COM backlog bytes:", "COM-Rückstau Bytes:") & Backlog.ToString("N0") & " (max " & BacklogMax.ToString("N0") & ")" & vbCrLf &
                         LBL("Ring buffer messages:", "Ringpuffer Nachr.:") & RingFill.ToString("N0") & " (max " & RingFillMax.ToString("N0") & ")" & vbCrLf &
                         LBL("COM overflows:", "COM-Überläufe:") & COM.Overflows & vbCrLf &
                         LBL("COM invalid lines:", "COM-ungültige Zeilen:") & COM.InvalidLines & vbCrLf &
                         LBL("Frames/s:", "Frames/s:") & Fps.ToString("0.0") & vbCrLf &
                         VDP.STATUS & vbCrLf &
                         VDP.MISMATCHLOG

    End Sub

    '******************************************************************************************************
    '*** VDP - Evaluate the Z80 messages to the VDP and fill VRAM and registers                         ***
    '******************************************************************************************************

    ' Synchronised once the MSX's RESET message has been received after connecting

    Private Synced As Boolean = False

    Private Sub VDP_In(sender As Object, e As EventArgs) Handles VDP_Timer_In.Tick

        Dim IOADR As Integer
        Dim IODATA As Integer
        Dim IODIR As Integer
        Dim Msg As Integer

        ' Record buffer levels before processing and update the status display once per second

        Messung_Auswerten()

        While COM.Available()

            ' Fetch the next message

            Msg = COM.GetMsg()

            If (Msg And clCOM.MSG_TIME) <> 0 Then

                ' Time mark of the sender

                If Synced Then VDP.TIMEMARK(Msg And &HFFFFFF)
                Continue While

            End If

            IOADR = (Msg >> 16) And &HFF
            IODIR = (Msg >> 8) And &H1
            IODATA = Msg And &HFF

            If IOADR = clCOM.MSG_RESET Then

                ' RESET of the MSX: synchronise the starting point

                Synced = True

                VDP.RESET()

            ElseIf Not Synced Then

                ' Before the first RESET the state of the MSX is unknown, discard the message

            ElseIf IODIR = 1 Then

                ' Write to the VDP

                VDP.IO_WRITE(IOADR, IODATA)

            Else

                ' Read from the VDP, IODATA is the value returned by the real VDP

                VDP.IO_READ(IOADR, IODATA)

            End If

        End While

        ' Output the last built frame

        VDP.PRESENT()

    End Sub


    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    '+++ COM                                                                                            +++
    '+++ Functions for the COM interface                                                                +++
    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    '******************************************************************************************************
    '*** COM - Open the interface                                                                       ***
    '******************************************************************************************************

    Public Sub COM_Oeffnen()

        If Not COM.Oeffnen() Then Exit Sub

        ' Set buttons and menu items

        frmMain_ToolStripMenuItem_Verbinden.Enabled = False
        frmMain_ToolStripMenuItem_Trennen.Enabled = True
        frmMain_ToolStripMenuItem_Einstellen.Enabled = False

        ' Initialise everything

        VDP.Clear()

        Messung_Zuruecksetzen()

        MSX_Reset()

        ' Update the main window

        Video_Menu()

    End Sub

    '******************************************************************************************************
    '*** COM - RESET of the MSX                                                                         ***
    '******************************************************************************************************

    ' Requests a RESET of the MSX; all messages are discarded until its RESET message synchronises
    ' the starting point

    Private Sub MSX_Reset()

        Synced = False

        COM.Msg_Send("7")

        rtbStatus.Text = vbCrLf & UI(" Waiting for the RESET message of the MSX ...", " Warte auf RESET-Nachricht des MSX ...")

    End Sub

    '******************************************************************************************************
    '*** COM - Close the interface                                                                    ***
    '******************************************************************************************************

    Public Sub COM_Schliessen()

        COM.Schliessen()

        If VIDEO.Recording Then Video_Stop()

        Synced = False

        ' Set buttons and menu items

        frmMain_ToolStripMenuItem_Verbinden.Enabled = True
        frmMain_ToolStripMenuItem_Trennen.Enabled = False
        frmMain_ToolStripMenuItem_Einstellen.Enabled = True

        ' Reset the VDP

        VDP.Clear()

        VDP.PRESENT()

        rtbStatus.Text = ""

        pbBACK.BackColor = VDP.CHIPCOLORTABLE(VDP.COLOR_BLACK)

        ' Update the main window

        Video_Menu()

    End Sub


End Class
