Imports System.IO
Imports System.ComponentModel
Imports System.Threading


Public Class clCOM

    '*******************************************************************************************************************
    '**** Interface variables for COM                                                                               ****
    '*******************************************************************************************************************

    ' COM variables

    Private _Client As New System.IO.Ports.SerialPort

    Public ReadOnly Property Client As System.IO.Ports.SerialPort
        Get
            Return _Client
        End Get
    End Property

    ' Interface name: COM port name or "-"

    Private _PortName As String = "-"
    Public Property PortName As String
        Get
            Return _PortName
        End Get
        Set(ByVal value As String)
            _PortName = value
            If value <> "-" Then _Client.PortName = value
        End Set
    End Property

    ' Display text of the interface

    Public ReadOnly Property Description As String
        Get
            Return "COM: " & _PortName & "-8N1-" & _Client.BaudRate
        End Get
    End Property

    ' Ring buffer for IO messages
    ' Entry: bits 16..23 = IO address (&H98/&H99) or MSG_RESET, bit 8 = direction (1 = write),
    ' bits 0..7 = data

    Public Const MSG_RESET As Integer = &H0

    ' Time mark: bit 30 set, bits 0..23 = sender's µs counter

    Public Const MSG_TIME As Integer = &H40000000

    Private Const _MsgMax As Integer = 1048576
    Private _Msg(_MsgMax - 1) As Integer
    Private _MsgIn As Integer
    Private _MsgOut As Integer

    ' Line buffer of the byte parser

    Private Const _LineMax As Integer = 8
    Private _Line(_LineMax - 1) As Byte
    Private _LineLen As Integer

    ' Diagnostic counters

    Private _Overflows As Long
    Public ReadOnly Property Overflows As Long
        Get
            Return Interlocked.Read(_Overflows)
        End Get
    End Property

    Private _InvalidLines As Long
    Public ReadOnly Property InvalidLines As Long
        Get
            Return Interlocked.Read(_InvalidLines)
        End Get
    End Property

    ' Throughput counters: received bytes and received lines

    Private _RxBytes As Long
    Public ReadOnly Property RxBytes As Long
        Get
            Return Interlocked.Read(_RxBytes)
        End Get
    End Property

    Private _RxMessages As Long
    Public ReadOnly Property RxMessages As Long
        Get
            Return Interlocked.Read(_RxMessages)
        End Get
    End Property

    ' Bytes not yet read in the COM driver

    Public ReadOnly Property Backlog As Integer
        Get
            Try
                Return _Client.BytesToRead
            Catch ex As Exception
                Return 0
            End Try
        End Get
    End Property

    ' Number of messages not yet processed in the ring buffer

    Public ReadOnly Property RingFill As Integer
        Get
            Dim Fill As Integer = Volatile.Read(_MsgIn) - Volatile.Read(_MsgOut)
            If Fill < 0 Then Fill += _MsgMax
            Return Fill
        End Get
    End Property

    Private ThreadReceive As System.Threading.Thread
    Private ThreadIsActive As Boolean = False

    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    '+++ COM                                                                                            +++
    '+++ Functions for the COM interface                                                                +++
    '++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    '******************************************************************************************************
    '*** COM - Open the interface                                                                       ***
    '******************************************************************************************************

    Public Function Oeffnen() As Boolean

        _Client.ReadBufferSize = 10000000
        _Client.ReadTimeout = 1000
        _Client.WriteTimeout = 1000

        ' Open the COM interface

        Try

            _Client.Open()
            _Client.DiscardInBuffer()

        Catch ex As Exception

            MsgBox(UI("The COM port (" & _PortName & ") could not be opened!", "Der COM-Port für die COM-Kommunikation (" & _PortName & ") konnte nicht geöffnet werden!"), vbOKOnly, "MSX - COM")

            Oeffnen = False
            Exit Function

        End Try

        ' Reset ring buffer, line buffer and diagnostic counters

        Volatile.Write(_MsgIn, 0)
        Volatile.Write(_MsgOut, 0)
        _LineLen = 0
        Interlocked.Exchange(_Overflows, 0)
        Interlocked.Exchange(_InvalidLines, 0)
        Interlocked.Exchange(_RxBytes, 0)
        Interlocked.Exchange(_RxMessages, 0)

        ' Start the receive thread

        Volatile.Write(ThreadIsActive, True)
        ThreadReceive = New System.Threading.Thread(AddressOf Thread_Msg_Receive)
        ThreadReceive.IsBackground = True
        ThreadReceive.Start()

        Oeffnen = True

    End Function

    '******************************************************************************************************
    '*** COM - Close the interface                                                                      ***
    '******************************************************************************************************

    Public Sub Schliessen()

        ' Stop the receive thread, closing the interface ends a blocking read

        Volatile.Write(ThreadIsActive, False)

        ' Close the COM port

        If _Client.IsOpen Then

            Try
                _Client.Close()
            Catch ex As Exception

            End Try

        End If

        If ThreadReceive IsNot Nothing Then

            ThreadReceive.Join(2000)
            ThreadReceive = Nothing

        End If

    End Sub

    '******************************************************************************************************
    '*** COM - Message available                                                                        ***
    '******************************************************************************************************
    Public Function Available() As Boolean

        Available = Volatile.Read(_MsgOut) <> Volatile.Read(_MsgIn)

    End Function

    '******************************************************************************************************
    '*** COM - Fetch a message                                                                          ***
    '******************************************************************************************************
    Public Function GetMsg() As Integer

        Dim MsgOut As Integer = Volatile.Read(_MsgOut)

        GetMsg = _Msg(MsgOut)

        ' Advance the counter of processed messages
        ' Wrap the counter to 0 at the end of the array

        MsgOut += 1
        If MsgOut = _MsgMax Then MsgOut = 0

        Volatile.Write(_MsgOut, MsgOut)

    End Function

    '******************************************************************************************************
    '*** COM - Receive thread                                                                           ***
    '******************************************************************************************************

    Private Sub Thread_Msg_Receive()

        Dim RxBuffer(65535) As Byte
        Dim Count As Integer

        ' Run the thread until a disconnect is requested

        While Volatile.Read(ThreadIsActive)

            ' Read a data block (blocks until data arrives or the timeout expires)

            Try

                Count = _Client.BaseStream.Read(RxBuffer, 0, RxBuffer.Length)

            Catch ex As TimeoutException

                Continue While

            Catch ex As Exception

                Exit While

            End Try

            If Count = 0 Then Continue While

            Interlocked.Add(_RxBytes, Count)

            Bytes_Auswerten(RxBuffer, Count)

        End While

    End Sub

    '******************************************************************************************************
    '*** COM - Split received bytes into lines and parse them                                           ***
    '******************************************************************************************************

    ' Lines end with LF. Valid line: type + 2 hex digits, RESET "7" alone or time mark
    ' "8" + 6 hex digits.
    ' 3 = read &H98, 4 = read &H99, 5 = write &H98, 6 = write &H99, 7 = RESET of the MSX,
    ' 8 = time mark (sender's µs counter, 24 bits)

    Private Sub Bytes_Auswerten(ByVal Buffer() As Byte, ByVal Count As Integer)

        For i As Integer = 0 To Count - 1

            Dim B As Byte = Buffer(i)

            If B <> 10 Then

                ' Collect characters, lines that are too long are discarded at the LF

                If _LineLen < _LineMax Then _Line(_LineLen) = B
                If _LineLen <= _LineMax Then _LineLen += 1
                Continue For

            End If

            Interlocked.Increment(_RxMessages)

            Dim LineLen As Integer = _LineLen
            _LineLen = 0

            Dim IOADR As Integer
            Dim IODIR As Integer
            Dim IODATA As Integer

            If LineLen = 1 AndAlso _Line(0) = AscW("7"c) Then

                IOADR = MSG_RESET
                IODIR = 0
                IODATA = 0

            ElseIf LineLen = 7 AndAlso _Line(0) = AscW("8"c) Then

                Dim T24 As Integer = 0

                For K As Integer = 1 To 6
                    Dim V As Integer = HexWert(_Line(K))
                    If V < 0 Then
                        T24 = -1
                        Exit For
                    End If
                    T24 = (T24 << 4) Or V
                Next

                If T24 < 0 Then
                    Interlocked.Increment(_InvalidLines)
                    Continue For
                End If

                MSG_ABLEGEN(MSG_TIME Or T24)
                Continue For

            ElseIf LineLen = 3 Then

                Dim Hi As Integer = HexWert(_Line(1))
                Dim Lo As Integer = HexWert(_Line(2))

                If Hi < 0 Or Lo < 0 Then

                    Interlocked.Increment(_InvalidLines)
                    Continue For

                End If

                IODATA = (Hi << 4) Or Lo

                Select Case _Line(0)

                    Case AscW("3"c)
                        IOADR = &H98
                        IODIR = 0

                    Case AscW("4"c)
                        IOADR = &H99
                        IODIR = 0

                    Case AscW("5"c)
                        IOADR = &H98
                        IODIR = 1

                    Case AscW("6"c)
                        IOADR = &H99
                        IODIR = 1

                    Case AscW("7"c)
                        IOADR = MSG_RESET
                        IODIR = 0
                        IODATA = 0

                    Case Else
                        Interlocked.Increment(_InvalidLines)
                        Continue For

                End Select

            Else

                Interlocked.Increment(_InvalidLines)
                Continue For

            End If

            MSG_ABLEGEN((IOADR << 16) Or (IODIR << 8) Or IODATA)

        Next

    End Sub

    ' Store the message in the ring buffer, count an overflow if it is full

    Private Sub MSG_ABLEGEN(ByVal MSG As Integer)

        Dim MsgIn As Integer = Volatile.Read(_MsgIn)
        Dim MsgNext As Integer = MsgIn + 1
        If MsgNext = _MsgMax Then MsgNext = 0

        If MsgNext = Volatile.Read(_MsgOut) Then

            Interlocked.Increment(_Overflows)

        Else

            _Msg(MsgIn) = MSG
            Volatile.Write(_MsgIn, MsgNext)

        End If

    End Sub

    Private Shared Function HexWert(ByVal B As Byte) As Integer

        Select Case B
            Case 48 To 57 : Return B - 48
            Case 65 To 70 : Return B - 55
            Case 97 To 102 : Return B - 87
            Case Else : Return -1
        End Select

    End Function

    '******************************************************************************************************
    '*** COM - Send a message                                                                           ***
    '******************************************************************************************************
    Public Sub Msg_Send(ByVal Msg As String)

        Dim Data() As Byte = System.Text.Encoding.ASCII.GetBytes(Msg & vbLf)

        ' Send over COM if open

        If _Client.IsOpen Then

            Try
                _Client.Write(Data, 0, Data.Length)
            Catch ex As Exception
                Schliessen()
                MsgBox(UI("The COM port (" & _PortName & ") is closed because the interface cannot be reached! Please check the settings!", "Der COM-Port für die COM-Kommunikation (" & _PortName & ") wird geschlossen da das Interface nicht erreichbar ist! Bitte die Einstellungen prüfen!"), vbOKOnly, "MSX - COM")
            End Try

        End If

    End Sub

    '******************************************************************************************************
    '*** COM - Load the settings                                                                        ***
    '******************************************************************************************************

    Public Sub New()
        Dim RecFile As FileStream
        Dim RecFileRd As StreamReader

        With _Client
            .BaudRate = 1000000
            .Parity = IO.Ports.Parity.None
            .StopBits = IO.Ports.StopBits.One
            .DtrEnable = False
            .RtsEnable = False
            .DataBits = 8
            .ReadTimeout = 1000
            .WriteTimeout = 1000
        End With

        ' Create the folder if it does not exist

        If System.IO.Directory.Exists("Inifiles") = False Then

            System.IO.Directory.CreateDirectory("Inifiles")

        End If

        If File.Exists("Inifiles/tms9918a_init.txt") Then
            RecFile = New FileStream("Inifiles/tms9918a_init.txt", FileMode.Open)
            RecFileRd = New StreamReader(RecFile)
            PortName = RecFileRd.ReadLine
            If RecFileRd.ReadLine = "de" Then UiText.Language = "de"
            If RecFileRd.ReadLine = "NTSC" Then clVIDEO.NTSC = True
            RecFileRd.Close()
            RecFile.Close()
        End If

    End Sub

    '******************************************************************************************************
    '*** COM - Save the settings                                                                        ***
    '******************************************************************************************************

    Public Sub Save()
        Dim RecFile As FileStream
        Dim RecFileWr As StreamWriter

        ' Create the folder if it does not exist

        If System.IO.Directory.Exists("Inifiles") = False Then

            System.IO.Directory.CreateDirectory("Inifiles")

        End If

        If File.Exists("Inifiles/tms9918a_init.txt") Then
            File.Delete("Inifiles/tms9918a_init.txt")
        End If

        RecFile = New FileStream("Inifiles/tms9918a_init.txt", FileMode.Create)
        RecFileWr = New StreamWriter(RecFile)
        RecFileWr.WriteLine(_PortName)
        RecFileWr.WriteLine(UiText.Language)
        RecFileWr.WriteLine(If(clVIDEO.NTSC, "NTSC", "PAL"))
        RecFileWr.Close()
        RecFile.Close()
    End Sub

End Class
