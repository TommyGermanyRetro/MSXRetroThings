Imports System.Runtime.InteropServices
Imports System.Text

Public Class clVIDEO

    '******************************************************************************************************
    '*** VIDEO - Interface to the native recorder (TMS9918AVideo.dll, shared VIDEOREC library)         ***
    '******************************************************************************************************

    Private Const DLL As String = "TMS9918AVideo.dll"

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Function vr_create() As IntPtr
    End Function

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Sub vr_destroy(ByVal Recorder As IntPtr)
    End Sub

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl, CharSet:=CharSet.Unicode)>
    Private Shared Function vr_start(ByVal Recorder As IntPtr, ByVal Path As String, ByVal Width As UInteger, ByVal Height As UInteger,
                                     ByVal Numerator As UInteger, ByVal Denominator As UInteger, ByVal SampleRate As UInteger,
                                     ByVal ErrorText As StringBuilder, ByVal ErrorLength As Integer) As Integer
    End Function

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Sub vr_frame(ByVal Recorder As IntPtr, ByVal Pixels() As Integer, ByVal Stride As UInteger)
    End Sub

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Sub vr_audio(ByVal Recorder As IntPtr, ByVal Samples() As Short, ByVal Count As UInteger)
    End Sub

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl, CharSet:=CharSet.Unicode)>
    Private Shared Function vr_stop(ByVal Recorder As IntPtr, ByVal ErrorText As StringBuilder, ByVal ErrorLength As Integer) As Integer
    End Function

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Function vr_recording(ByVal Recorder As IntPtr) As Integer
    End Function

    <DllImport(DLL, CallingConvention:=CallingConvention.Cdecl)>
    Private Shared Function vr_seconds(ByVal Recorder As IntPtr) As Double
    End Function

    '******************************************************************************************************
    '*** VIDEO - Recording                                                                              ***
    '******************************************************************************************************

    ' VDP master clock; the frame rate of the video is MASTER_CLOCK / master ticks per field

    Public Const MASTER_CLOCK As UInteger = 21477270

    ' The VDP has no sound; the MP4 still gets a silent sound track so that picture and sound stay
    ' in step in every player

    Private Const SAMPLE_RATE As UInteger = 44100

    ' Video standard of the recording, chosen in the menu and saved with the COM settings; the
    ' standard detected from the stream is unknown again after every RESET of the MSX

    Public Shared NTSC As Boolean = False

    ' Master clock ticks per field of the chosen standard (313 or 262 lines of 1368 ticks)
    Public Shared ReadOnly Property STANDARDTICKS As Integer
        Get
            Return If(NTSC, 262, 313) * 1368
        End Get
    End Property

    ' Border around the picture in the backdrop colour, as in the main window

    Public Const BORDER As Integer = 10

    Private _Recorder As IntPtr = IntPtr.Zero
    Private _Pixels() As Integer
    Private _Silence() As Short
    Private _FieldTicks As Integer
    Private _SampleRest As Long

    Public ReadOnly Property Width As Integer
        Get
            Return clVDP.SCR_W * clVDP.ZOOM + 2 * BORDER
        End Get
    End Property

    Public ReadOnly Property Height As Integer
        Get
            Return clVDP.SCR_H * clVDP.ZOOM + 2 * BORDER
        End Get
    End Property

    Public ReadOnly Property Recording As Boolean
        Get
            Return _Recorder <> IntPtr.Zero AndAlso vr_recording(_Recorder) <> 0
        End Get
    End Property

    ' Master clock ticks per field the recording was started with
    Public ReadOnly Property FieldTicks As Integer
        Get
            Return _FieldTicks
        End Get
    End Property

    ' Recorded time in seconds (time of the fields written so far)
    Public ReadOnly Property Seconds As Double
        Get
            If _Recorder = IntPtr.Zero Then Return 0
            Return vr_seconds(_Recorder)
        End Get
    End Property

    ' Opens the MP4 file; returns an empty string on success, otherwise the error message
    Public Function Start(ByVal Path As String, ByVal FieldTicks As Integer) As String

        Dim ErrorText As New StringBuilder(1024)

        Try

            If _Recorder = IntPtr.Zero Then _Recorder = vr_create()
            If _Recorder = IntPtr.Zero Then Return "vr_create"

            ReDim _Pixels(Width * Height - 1)
            ReDim _Silence(CInt(SAMPLE_RATE \ 10))
            _FieldTicks = FieldTicks
            _SampleRest = 0

            If vr_start(_Recorder, Path, CUInt(Width), CUInt(Height), MASTER_CLOCK, CUInt(FieldTicks), SAMPLE_RATE, ErrorText, ErrorText.Capacity) = 0 Then
                Return ErrorText.ToString()
            End If

        Catch ex As Exception

            Return ex.Message

        End Try

        Return ""

    End Function

    ' Hands the last finished frame of the VDP over COUNT times, each with the silence of one field
    Public Sub AddFields(ByVal VDP As clVDP, ByVal COUNT As Long)

        If Not Recording Then Exit Sub

        VDP.VIDEOFRAME(_Pixels, BORDER)

        For I As Long = 1 To COUNT

            vr_frame(_Recorder, _Pixels, CUInt(Width))

            ' Samples of one field, the remainder carried over to the next field

            _SampleRest += CLng(SAMPLE_RATE) * _FieldTicks
            Dim Samples As Long = _SampleRest \ MASTER_CLOCK
            _SampleRest -= Samples * MASTER_CLOCK

            While Samples > 0
                Dim Part As Integer = CInt(Math.Min(Samples, _Silence.Length))
                vr_audio(_Recorder, _Silence, CUInt(Part))
                Samples -= Part
            End While

        Next

    End Sub

    ' Finishes the file; returns an empty string on success, otherwise the error message
    Public Function [Stop]() As String

        If _Recorder = IntPtr.Zero Then Return ""

        Dim ErrorText As New StringBuilder(1024)
        Dim Result As String = ""

        If vr_stop(_Recorder, ErrorText, ErrorText.Capacity) = 0 Then Result = ErrorText.ToString()

        vr_destroy(_Recorder)
        _Recorder = IntPtr.Zero

        Return Result

    End Function

End Class
