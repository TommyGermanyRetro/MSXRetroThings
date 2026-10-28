Imports System.Drawing.Imaging
Imports System.Runtime.InteropServices

Public Class clVDP

    '******************************************************************************************************
    '*** VDP - Variable definitions                                                                     ***
    '******************************************************************************************************

    '******************************************************************************************************
    '* VDP registers and derived settings                                                                 *
    '******************************************************************************************************

    Private _REGISTER(7) As Integer
    Public ReadOnly Property REGISTER(ByVal index As Integer) As Integer
        Get
            Return _REGISTER(index)
        End Get
    End Property

    Private _MODETABLE(8) As String
    Private _VRAMSIZETABLE(2) As String
    Private _SPRITESIZETABLE(2) As String
    Private _SPRITEMAGNIFICATIONTABLE(2) As String
    Private _M1 As Integer
    Private _M2 As Integer
    Private _M3 As Integer
    Private _MODE As Integer
    Private _IE As Integer
    Private _EXTVIDEO As Integer
    Private _SCREENACTIVE As Integer
    Private _VRAMSIZE As Integer
    Private _SPRITESIZE As Integer
    Private _SPRITEMAGNIFICATION As Integer
    Public ReadOnly Property STATUS As String
        Get
            Return LBL("Register 0:", "Register 0:") & "&H" & _REGISTER(0).ToString("X2") & vbCrLf &
                   LBL("Register 1:", "Register 1:") & "&H" & _REGISTER(1).ToString("X2") & vbCrLf &
                   LBL("Register 2:", "Register 2:") & "&H" & _REGISTER(2).ToString("X2") & vbCrLf &
                   LBL("Register 3:", "Register 3:") & "&H" & _REGISTER(3).ToString("X2") & vbCrLf &
                   LBL("Register 4:", "Register 4:") & "&H" & _REGISTER(4).ToString("X2") & vbCrLf &
                   LBL("Register 5:", "Register 5:") & "&H" & _REGISTER(5).ToString("X2") & vbCrLf &
                   LBL("Register 6:", "Register 6:") & "&H" & _REGISTER(6).ToString("X2") & vbCrLf &
                   LBL("Register 7:", "Register 7:") & "&H" & _REGISTER(7).ToString("X2") & vbCrLf &
                   LBL("VRAM address:", "VRAM-Adresse:") & "&H" & _VRAMADR.ToString("X4") & vbCrLf &
                   LBL("Latch:", "Latch:") & If(_PROGSTEP = 1, "&H" & _PROGDATA.ToString("X2"), "-") & vbCrLf &
                   LBL("Read-ahead:", "Read-Ahead:") & "&H" & _READBUF.ToString("X2") & vbCrLf &
                   LBL("Status (real):", "Status (echt):") & "&H" & _STATUSREAL.ToString("X2") & vbCrLf &
                   LBL("Status (emulated):", "Status (emuliert):") & "&H" & _STATEMU.ToString("X2") & vbCrLf &
                   LBL("Status compares:", "Status-Vergleiche:") & _STATCOMPARED & vbCrLf &
                   LBL("Status mismatches:", "Status-Abweichungen:") & _STATMISMATCHES & vbCrLf &
                   LBL("Status not comparable:", "Status n. vergleichbar:") & _STATSKIPPED & vbCrLf &
                   LBL("Time base:", "Zeitbezug:") & If(Not _TMODE, UI("off", "aus"), If(Not _PHASE, UI("waiting", "wartet"), If(Not _LOCKED, UI("acquiring", "Erfassung"), If(_NLINES = 313, "PAL", "NTSC")))) & vbCrLf &
                   LBL("Phase corrections:", "Phasenkorrekturen:") & _PHASEFIX & vbCrLf &
                   LBL("RESETs:", "RESETs:") & _RESETS & vbCrLf &
                   LBL("Frames (VBLANK):", "Frames (VBLANK):") & _FRAMES & vbCrLf &
                   LBL("VRAM reads:", "VRAM-Lesezugriffe:") & _VRAMREADS & vbCrLf &
                   LBL("VRAM mismatches:", "VRAM-Abweichungen:") & _MISMATCHES & vbCrLf &
                   LBL("NAMETABLE:", "NAMETABLE:") & "&H" & _NAMETABLE.ToString("X4") & vbCrLf &
                   LBL("COLORTABLE:", "COLORTABLE:") & "&H" & _COLORTABLE.ToString("X4") & vbCrLf &
                   LBL("PATTERNTABLE:", "PATTERNTABLE:") & "&H" & _PATTERNTABLE.ToString("X4") & vbCrLf &
                   LBL("SPRITEATTRTABLE:", "SPRITEATTRTABLE:") & "&H" & _SPRITEATTRTABLE.ToString("X4") & vbCrLf &
                   LBL("SPRITEPATTERNTABLE:", "SPRITEPATTERNTABLE:") & "&H" & _SPRITEPATTERNTABLE.ToString("X4") & vbCrLf &
                   LBL("COLORTXT:", "COLORTXT:") & "&H" & _COLORTXT.ToString("X2") & vbCrLf &
                   LBL("COLORBACK:", "COLORBACK:") & "&H" & _COLORBACK.ToString("X2") & vbCrLf &
                   LBL("Screen mode:", "Bildmodus:") & _MODETABLE(_MODE) & If(_MODE = 3 OrElse _MODE >= 5, UI(" (undoc.)", " (undok.)"), "") & vbCrLf &
                   LBL("Display:", "Bild:") & If(_SCREENACTIVE = 1, UI("active", "an"), UI("blank", "aus")) & vbCrLf &
                   LBL("VRAM size:", "VRAM-Größe:") & _VRAMSIZETABLE(_VRAMSIZE) & vbCrLf &
                   LBL("Ext. video:", "Ext. Video:") & If(_EXTVIDEO = 1, UI("enabled", "an"), UI("disabled", "aus")) & vbCrLf &
                   LBL("Interrupt:", "Interrupt:") & If(_IE = 1, UI("enabled", "an"), UI("disabled", "aus")) & vbCrLf &
                   LBL("Sprite size:", "Spritegröße:") & _SPRITESIZETABLE(_SPRITESIZE) & vbCrLf &
                   LBL("Sprite magnif.:", "Spritevergr.:") & _SPRITEMAGNIFICATIONTABLE(_SPRITEMAGNIFICATION) & vbCrLf &
                   LBL("MODE:", "MODE:") & _MODE
        End Get
    End Property

    ' Label of a status line, padded to the value column

    Private Shared Function LBL(ByVal EN As String, ByVal DE As String) As String
        Return (" " & UI(EN, DE)).PadRight(24)
    End Function

    '******************************************************************************************************
    '* VDP VRAM and state machine variables                                                               *
    '******************************************************************************************************

    ' VRAM

    Private _VRAMARRAY(16383) As Integer
    Public ReadOnly Property VRAMARRAY(ByVal index As Integer) As Integer
        Get
            Return _VRAMARRAY(index)
        End Get
    End Property

    ' VRAM index

    Private _VRAMADR As Integer

    Public ReadOnly Property VRAMADR As Integer
        Get
            Return _VRAMADR
        End Get
    End Property

    ' VRAM-NAMETABLE

    Private _NAMETABLE As Integer
    Public ReadOnly Property NAMETABLE As Integer
        Get
            Return _NAMETABLE
        End Get
    End Property

    ' VRAM-COLORTABLE

    Private _COLORTABLE As Integer
    Public ReadOnly Property COLORTABLE As Integer
        Get
            Return _COLORTABLE
        End Get
    End Property

    ' VRAM-PATTERNTABLE

    Private _PATTERNTABLE As Integer
    Public ReadOnly Property PATTERNTABLE As Integer
        Get
            Return _PATTERNTABLE
        End Get
    End Property

    ' VRAM-SPRITEATTRTABLE

    Private _SPRITEATTRTABLE As Integer
    Public ReadOnly Property SPRITEATTRTABLE As Integer
        Get
            Return _SPRITEATTRTABLE
        End Get
    End Property

    ' VRAM-SPRITEPATTERNTABLE

    Private _SPRITEPATTERNTABLE As Integer
    Public ReadOnly Property SPRITEPATTERNTABLE As Integer
        Get
            Return _SPRITEPATTERNTABLE
        End Get
    End Property

    ' VRAM-COLORTXT

    Private _COLORTXT As Integer
    Public ReadOnly Property COLORTXT As Integer
        Get
            Return _COLORTXT
        End Get
    End Property

    ' VRAM-COLORBACK

    Private _COLORBACK As Integer
    Public ReadOnly Property COLORBACK As Integer
        Get
            Return _COLORBACK
        End Get
    End Property

    ' Programming step index (0 = next byte to port &H99 is the first one, 1 = the second one)

    Private _PROGSTEP As Integer

    ' Latch for programming step 0

    Private _PROGDATA As Integer

    Private _START As Integer

    ' Read-ahead buffer and the physical VRAM address its content comes from

    Private _READBUF As Integer
    Private _READBUFPHYS As Integer

    ' Status register last read from the real VDP

    Private _STATUSREAL As Integer
    Public ReadOnly Property STATUSREAL As Integer
        Get
            Return _STATUSREAL
        End Get
    End Property

    ' Number of VBLANK frames (status register read with the F bit set)

    Private _FRAMES As Long
    Public ReadOnly Property FRAMES As Long
        Get
            Return _FRAMES
        End Get
    End Property

    ' Comparison of the real VDP's VRAM read data with the read-ahead buffer

    Private _VRAMREADS As Long
    Public ReadOnly Property VRAMREADS As Long
        Get
            Return _VRAMREADS
        End Get
    End Property

    Private _MISMATCHES As Long
    Public ReadOnly Property MISMATCHES As Long
        Get
            Return _MISMATCHES
        End Get
    End Property

    ' Log of the first mismatches: VRAM address, expected, real, previous IO access

    Private Const _MISMATCHLOGMAX As Integer = 8
    Private _MISMATCHLOG As New List(Of String)
    Public ReadOnly Property MISMATCHLOG As String
        Get
            Return String.Join(vbCrLf, _MISMATCHLOG.Concat(_STATLOG))
        End Get
    End Property


    ' Previous IO access for the mismatch log

    Private _LASTIO As String = ""

    ' Emulated status register and capture of the current frame

    Private _STATEMU As Integer
    Public ReadOnly Property STATEMU As Integer
        Get
            Return _STATEMU
        End Get
    End Property

    Private _FRM5S As Integer = -1
    Private _FRMCOLL As Boolean

    ' Text mode or blanked display since the last status read: 5S and bits 4-0 are undefined

    Private _STATUNDEF As Boolean

    ' Time base from the sender's time marks (µs counter, 24 bits)
    ' The frame position is derived from the status reads with the F bit set (only the VDP sets F),
    ' it follows the shortest delay of the most recent reads.

    Private Const LINE_US As Double = 342.0 / 5.3693175

    Private _TMODE As Boolean
    Private _TLAST24 As Integer = -1
    Private _TNOW As Long
    Private _NLINES As Integer
    Private _PHASE As Boolean
    Private _TVPREV As Double
    Private _NEXTLINE As Integer
    Private _PHASEFIX As Long

    ' Comparison with the real VDP's status register when read at the end of a frame

    Private _STATPOLLED As Boolean
    Private _SATWRITTEN As Boolean
    Private _STATCOMPARED As Long
    Private _STATSKIPPED As Long
    Private _STATMISMATCHES As Long
    Public ReadOnly Property STATMISMATCHES As Long
        Get
            Return _STATMISMATCHES
        End Get
    End Property
    Public ReadOnly Property STATCOMPARED As Long
        Get
            Return _STATCOMPARED
        End Get
    End Property

    Private Const _STATLOGMAX As Integer = 8
    Private _STATLOG As New List(Of String)

    '******************************************************************************************************
    '* Graphics variables                                                                                 *
    '******************************************************************************************************

    ' Size of the active display area and output magnification

    Public Const SCR_W As Integer = 256
    Public Const SCR_H As Integer = 192
    Public Const ZOOM As Integer = 4

    ' Colour index per pixel of the frame last built at VBLANK

    Private _FRAME(SCR_W * SCR_H - 1) As Byte
    Private _FRAMEW(SCR_W * SCR_H - 1) As Byte
    Private _FRAMEDIRTY As Boolean

    ' Output image, written in the UI thread only

    Private _IMAGE As New Bitmap(SCR_W * ZOOM, SCR_H * ZOOM, PixelFormat.Format32bppRgb)
    Public ReadOnly Property IMAGE As Bitmap
        Get
            Return _IMAGE
        End Get
    End Property

    ' Line buffers: colour index per pixel, sprite colour (-1 = no sprite) and sprite pixel occupied

    Private _LINE(SCR_W - 1) As Integer
    Private _SPRCOLOR(SCR_W - 1) As Integer
    Private _SPRHIT(SCR_W - 1) As Boolean

    ' Line buffer for the output

    Private _ROWBUF(SCR_W * ZOOM - 1) As Integer

    ' VRAM addressed directly in 16K mode (otherwise translated through VRAMPHYS)

    Private _IS16K As Boolean

    ' VDP colour palette

    Private _CHIPCOLORTABLE(15) As Object

    Public ReadOnly Property CHIPCOLORTABLE(ByVal index As Integer) As Object
        Get
            Return _CHIPCOLORTABLE(index)
        End Get
    End Property

    Private _ARGB(15) As Integer

    Private _COLOR_TRANSPARENT = 0
    Public ReadOnly Property COLOR_TRANSPARENT As Integer
        Get
            Return _COLOR_TRANSPARENT
        End Get
    End Property

    Private _COLOR_BLACK = 1
    Public ReadOnly Property COLOR_BLACK As Integer
        Get
            Return _COLOR_BLACK
        End Get
    End Property

    Private _COLOR_GRAY = 14
    Public ReadOnly Property COLOR_GRAY As Integer
        Get
            Return _COLOR_GRAY
        End Get
    End Property

    Private _COLOR_WHITE = 15
    Public ReadOnly Property COLOR_WHITE As Integer
        Get
            Return _COLOR_WHITE
        End Get
    End Property

    '******************************************************************************************************
    '*** VDP - Prepare a new video processor                                                            ***
    '******************************************************************************************************
    Public Sub New()

        Dim index As Integer

        ' Initialise screen modes

        For index = 0 To 8
            _MODETABLE(index) = ""
        Next

        _MODETABLE(0) = "Screen I"
        _MODETABLE(1) = "Text"
        _MODETABLE(2) = "Multimode"
        _MODETABLE(3) = "Text+Multi"
        _MODETABLE(4) = "Screen II"
        _MODETABLE(5) = "Text+Screen II"
        _MODETABLE(6) = "Multi+Screen II"
        _MODETABLE(7) = "Text+Multi+SII"

        ' Initialise VRAM size

        _VRAMSIZETABLE(0) = "4k"
        _VRAMSIZETABLE(1) = "16k"

        ' Sprite size

        _SPRITESIZETABLE(0) = "8x8"
        _SPRITESIZETABLE(1) = "16x16"

        ' Sprite magnification

        _SPRITEMAGNIFICATIONTABLE(0) = "x1"
        _SPRITEMAGNIFICATIONTABLE(1) = "x2"

        ' Colour palette from the TI data manual (Y, R-Y, B-Y), converted to RGB after Sean Young / MESS
        ' Colour 0 is transparent and is replaced by the backdrop when the frame is built

        Dim RGBTABLE(,) As Integer = {
            {0, 0, 0},       ' 0 Transparent
            {0, 0, 0},       ' 1 Black
            {33, 200, 66},   ' 2 Medium Green
            {94, 220, 120},  ' 3 Light Green
            {84, 85, 237},   ' 4 Dark Blue
            {125, 118, 252}, ' 5 Light Blue
            {212, 82, 77},   ' 6 Dark Red
            {66, 235, 245},  ' 7 Cyan
            {252, 85, 84},   ' 8 Medium Red
            {255, 121, 120}, ' 9 Light Red
            {212, 193, 84},  ' A Dark Yellow
            {230, 206, 128}, ' B Light Yellow
            {33, 176, 59},   ' C Dark Green
            {201, 91, 186},  ' D Magenta
            {204, 204, 204}, ' E Gray
            {255, 255, 255}  ' F White
        }

        For index = 0 To 15
            Dim C As Color = Color.FromArgb(RGBTABLE(index, 0), RGBTABLE(index, 1), RGBTABLE(index, 2))
            _CHIPCOLORTABLE(index) = C
            _ARGB(index) = C.ToArgb()
        Next
        ' Reset all registers

        Clear()

    End Sub

    '******************************************************************************************************
    '*** VDP - Reset the video processor                                                                ***
    '******************************************************************************************************
    Public Sub Clear()

        ' Initialise the VDP

        _MODE = 0
        _M1 = 0
        _M2 = 0
        _M3 = 0
        _VRAMSIZE = 0
        _SCREENACTIVE = 0
        _EXTVIDEO = 0
        _IE = 0
        _SPRITESIZE = 0
        _SPRITEMAGNIFICATION = 0

        ' Reset registers

        For index = 0 To 7
            _REGISTER(index) = 0
        Next

        ' Reset VRAM

        For index = 0 To 16383
            _VRAMARRAY(index) = 0
        Next

        ' Reset address tables

        _NAMETABLE = 0
        _COLORTABLE = 0
        _PATTERNTABLE = 0
        _SPRITEATTRTABLE = 0
        _SPRITEPATTERNTABLE = 0
        _COLORTXT = 0
        _COLORBACK = 0

        ' Reset message state

        _PROGDATA = 0
        _PROGSTEP = 0
        _VRAMADR = 0
        _START = 0
        _READBUF = 0
        _READBUFPHYS = 0
        _STATUSREAL = 0
        _LASTIO = ""

        ' Reset diagnostic counters

        _RESETS = 0
        _FRAMES = 0
        _VRAMREADS = 0
        _MISMATCHES = 0
        _MISMATCHLOG.Clear()
        _STATEMU = 0
        _STATUNDEF = True
        _STATPOLLED = False
        _STATCOMPARED = 0
        _STATSKIPPED = 0
        _STATMISMATCHES = 0
        _STATLOG.Clear()

        ' Reset time base

        _TMODE = False
        _TLAST24 = -1
        _TNOW = 0
        _FTIMES.Clear()
        _NLINES = 0
        _PHASE = False
        _NEXTLINE = 0
        _PHASEFIX = 0
        _PHASED.Clear()

        ' Reset image

        Array.Clear(_FRAME, 0, _FRAME.Length)
        Array.Clear(_FRAMEW, 0, _FRAMEW.Length)
        _FRAMEDIRTY = True

    End Sub

    '******************************************************************************************************
    '*** VDP - Physical VRAM address from the VDP address                                               ***
    '******************************************************************************************************

    ' In 16K mode the VDP address equals the VRAM address. In 4K mode (register 1 bit 7 = 0) the VDP
    ' puts A12 into the column address and A13 into the row address of the DRAM:
    ' column AD0-AD5 = A0-A5, AD6 = A12; row AD0-AD5 = A6-A11, AD6 = A13

    Private Function VRAMPHYS(ByVal ADR As Integer) As Integer

        If (_REGISTER(1) And &H80) <> 0 Then

            VRAMPHYS = ADR

        Else

            VRAMPHYS = (ADR And &H3F) Or (((ADR >> 12) And &H1) << 6) Or (((ADR >> 6) And &H3F) << 7) Or (ADR And &H2000)

        End If

    End Function

    '******************************************************************************************************
    '*** VDP - Fill the read-ahead buffer from the current VRAM address                                 ***
    '******************************************************************************************************

    Private Sub READAHEAD()

        _READBUFPHYS = VRAMPHYS(_VRAMADR)
        _READBUF = _VRAMARRAY(_READBUFPHYS)
        _VRAMADR = (_VRAMADR + 1) And &H3FFF

    End Sub

    '******************************************************************************************************
    '*** VDP - Write a register and rebuild the derived settings from all registers                    ***
    '******************************************************************************************************

    Private Sub WRITEREGISTER(ByVal REG As Integer, ByVal VALUE As Integer)

        _REGISTER(REG) = VALUE

        If (_REGISTER(1) And &H10) <> 0 OrElse (_REGISTER(1) And &H40) = 0 Then _STATUNDEF = True

        ' Register 0

        _M3 = ((_REGISTER(0) >> 1) And &H1) << 2
        _EXTVIDEO = _REGISTER(0) And &H1

        ' Register 1

        _VRAMSIZE = (_REGISTER(1) >> 7) And &H1
        _SCREENACTIVE = (_REGISTER(1) >> 6) And &H1
        _IE = (_REGISTER(1) >> 5) And &H1
        _M1 = (_REGISTER(1) >> 4) And &H1
        _M2 = ((_REGISTER(1) >> 3) And &H1) << 1
        _SPRITESIZE = (_REGISTER(1) >> 1) And &H1
        _SPRITEMAGNIFICATION = _REGISTER(1) And &H1

        _MODE = _M1 Or _M2 Or _M3

        ' Register 2: name table

        _NAMETABLE = (_REGISTER(2) And &HF) << 10

        ' Registers 3 and 4: colour table and pattern generator table
        ' With M3 set only CT13 or PG13 select the base address, the remaining bits are masks

        If _M3 <> 0 Then

            _COLORTABLE = (_REGISTER(3) And &H80) << 6
            _PATTERNTABLE = (_REGISTER(4) And &H4) << 11

        Else

            _COLORTABLE = _REGISTER(3) << 6
            _PATTERNTABLE = (_REGISTER(4) And &H7) << 11

        End If

        ' Registers 5 and 6: sprite attribute table and sprite pattern generator table

        _SPRITEATTRTABLE = (_REGISTER(5) And &H7F) << 7
        _SPRITEPATTERNTABLE = (_REGISTER(6) And &H7) << 11

        ' Register 7: text colour and backdrop

        _COLORTXT = (_REGISTER(7) >> 4) And &HF
        _COLORBACK = _REGISTER(7) And &HF

    End Sub

    '******************************************************************************************************
    '*** VDP - IO access -> write                                                                       ***
    '******************************************************************************************************

    Public Sub IO_WRITE(ByVal IOADR As Integer, ByVal IODATA As Integer)

        ' Port &H98: write VRAM
        ' The written value also goes into the read-ahead buffer, then the address is incremented

        If IOADR = &H98 Then

            Dim SA As Integer = (_REGISTER(5) And &H7F) << 7
            If _VRAMADR >= SA And _VRAMADR < SA + 128 Then _SATWRITTEN = True

            _READBUFPHYS = VRAMPHYS(_VRAMADR)
            _VRAMARRAY(_READBUFPHYS) = IODATA
            _READBUF = IODATA
            _VRAMADR = (_VRAMADR + 1) And &H3FFF

            _PROGSTEP = 0

            _LASTIO = "W98 " & IODATA.ToString("X2")

        End If

        ' Port &H99: set a register or the VRAM address (two bytes)

        If IOADR = &H99 Then

            If _PROGSTEP = 0 Then

                ' First byte: latch the register value or A7-A0

                _PROGDATA = IODATA
                _PROGSTEP = 1

            Else

                ' Second byte

                _PROGSTEP = 0

                If (IODATA And &H80) <> 0 Then

                    ' Bit 7 set: register write, register number in bits 2-0

                    WRITEREGISTER(IODATA And &H7, _PROGDATA)

                Else

                    ' Bit 7 clear: set the VRAM address from A13-A8 (bits 5-0) and A7-A0 (first byte)
                    ' Bit 6 clear (read): fetch the VRAM content into the read-ahead buffer and increment the address

                    _VRAMADR = ((IODATA And &H3F) << 8) Or _PROGDATA

                    If (IODATA And &H40) = 0 Then

                        READAHEAD()

                    End If

                End If

            End If

            _LASTIO = "W99 " & IODATA.ToString("X2")

        End If

    End Sub

    '******************************************************************************************************
    '*** VDP - IO access -> read                                                                        ***
    '******************************************************************************************************

    ' IODATA is the value read from the real VDP

    Public Sub IO_READ(ByVal IOADR As Integer, ByVal IODATA As Integer)

        ' Port &H98: read VRAM
        ' The real VDP returns the read-ahead buffer, which is then refilled from the current address

        If IOADR = &H98 Then

            _VRAMREADS += 1

            If IODATA <> _READBUF Then

                _MISMATCHES += 1

                If _MISMATCHLOG.Count < _MISMATCHLOGMAX Then

                    _MISMATCHLOG.Add(" &H" & _READBUFPHYS.ToString("X4") & UI(" exp &H", " erw &H") & _READBUF.ToString("X2") & UI(" real &H", " echt &H") & IODATA.ToString("X2") & UI(" after ", " nach ") & _LASTIO)

                End If

            End If

            READAHEAD()

            _PROGSTEP = 0

            _LASTIO = "R98 " & IODATA.ToString("X2")

        End If

        ' Port &H99: read the status register
        ' If the F bit is set, the real VDP has reached the end of the active display area (VBLANK)

        If IOADR = &H99 Then

            _STATUSREAL = IODATA

            _PROGSTEP = 0

            _LASTIO = "R99 " & IODATA.ToString("X2")

            If _PHASE Then

                ' With time base: the lines up to now are processed, every read is compared

                If (IODATA And &H80) <> 0 Then PHASE_FREAD()

                If _LOCKED Then
                    STATUS_COMPARE(IODATA, UI(" line ", " Zeile ") & _NEXTLINE)
                Else
                    _STATSKIPPED += 1
                End If

            ElseIf (IODATA And &H80) <> 0 Then

                ' Without time base: frame at the time of the VBLANK read

                If _TMODE Then PHASE_START()

                _FRAMES += 1

                Out()
                FRAME_END()

                RaiseEvent VDP_Field(1)

                ' Compare only if the status register has not been read since the last end of frame
                ' (otherwise it is unknown when within the frame the flags were cleared)

                If _STATPOLLED Then

                    _STATSKIPPED += 1

                Else

                    STATUS_COMPARE(IODATA, If(_SATWRITTEN, UI(" (SAT changed during frame)", " (SAT im Bild geändert)"), ""))

                End If

                _STATPOLLED = False
                _SATWRITTEN = False

            Else

                _STATPOLLED = True

            End If

            ' Reading clears F, 5S and C

            _STATEMU = _STATEMU And &H1F

            _STATUNDEF = (_REGISTER(1) And &H10) <> 0 OrElse (_REGISTER(1) And &H40) = 0

        End If

    End Sub

    '******************************************************************************************************
    '*** VDP - RESET of the MSX                                                                         ***
    '******************************************************************************************************

    ' RESET clears registers 0 and 1, VRAM and registers 2 to 7 are retained.
    ' The diagnostic counters restart with the RESET.

    Private _RESETS As Long
    Public ReadOnly Property RESETS As Long
        Get
            Return _RESETS
        End Get
    End Property

    Public Sub RESET()

        WRITEREGISTER(0, 0)
        WRITEREGISTER(1, 0)

        _PROGSTEP = 0
        _PROGDATA = 0
        _STATUSREAL = 0
        _LASTIO = "RESET"

        _RESETS += 1
        _FRAMES = 0
        _VRAMREADS = 0
        _MISMATCHES = 0
        _MISMATCHLOG.Clear()
        _STATEMU = 0
        _STATUNDEF = True
        _STATPOLLED = False
        _STATCOMPARED = 0
        _STATSKIPPED = 0
        _STATMISMATCHES = 0
        _STATLOG.Clear()

        _PHASE = False
        _NLINES = 0
        _FTIMES.Clear()
        _PHASEFIX = 0
        _PHASED.Clear()

        Out()

    End Sub


    '******************************************************************************************************
    '*** VDP - Read VRAM for building the frame                                                         ***
    '******************************************************************************************************

    Private Function VR(ByVal ADR As Integer) As Integer

        ADR = ADR And &H3FFF
        If _IS16K Then Return _VRAMARRAY(ADR)
        Return _VRAMARRAY(VRAMPHYS(ADR))

    End Function

    '******************************************************************************************************
    '*** VDP - Process one display line                                                                 ***
    '******************************************************************************************************

    ' Y = 0..191: build the display line into the work buffer and check sprites, Y = 192: check sprites
    ' only (the VDP checks display lines 0 to 192). Sprites are checked only with the display enabled
    ' and not in text mode.
    ' Status rules per line: 5S only if F and 5S are still clear; without 5S bits 4-0 hold the number
    ' of the last sprite processed; C stays set until read.
    ' Colour 0 (transparent) shows the backdrop, backdrop 0 is black.

    Private Sub PROCESS_LINE(ByVal Y As Integer)

        Dim BD As Integer = _REGISTER(7) And &HF
        Dim BLANK As Boolean = (_REGISTER(1) And &H40) = 0
        Dim SPRITES As Boolean = (_REGISTER(1) And &H10) = 0

        _IS16K = (_REGISTER(1) And &H80) <> 0

        _FRM5S = -1
        _FRMCOLL = False

        If Y < SCR_H Then

            If BLANK Then

                Array.Clear(_LINE, 0, SCR_W)

            Else

                RENDER_LINE(Y)
                If SPRITES Then RENDER_SPRITES(Y, True)

            End If

            Dim O As Integer = Y * SCR_W

            For X As Integer = 0 To SCR_W - 1
                Dim C As Integer = _LINE(X)
                If C = 0 Then C = BD
                _FRAMEW(O + X) = CByte(C)
            Next

        ElseIf SPRITES And Not BLANK Then

            RENDER_SPRITES(Y, False)

        End If

        If SPRITES And Not BLANK Then

            If _FRM5S >= 0 AndAlso (_STATEMU And &HC0) = 0 Then
                _STATEMU = &H40 Or (_STATEMU And &H20) Or _FRM5S
            End If

            If (_STATEMU And &H40) = 0 Then
                _STATEMU = (_STATEMU And &HA0) Or LASTSPRITE()
            End If

            If _FRMCOLL Then _STATEMU = _STATEMU Or &H20

        End If

    End Sub

    ' Last sprite processed: position of the D0h terminator, otherwise 31

    Private Function LASTSPRITE() As Integer

        Dim SA As Integer = (_REGISTER(5) And &H7F) << 7

        For I As Integer = 0 To 31
            If VR(SA + I * 4) = &HD0 Then Return I
        Next

        Return 31

    End Function

    '******************************************************************************************************
    '*** VDP - Whole frame without time base                                                            ***
    '******************************************************************************************************

    ' Without time base the frame is built from VRAM and registers at the time of the VBLANK read

    Private Sub Out()

        For Y As Integer = 0 To SCR_H
            PROCESS_LINE(Y)
        Next

        Array.Copy(_FRAMEW, _FRAME, _FRAME.Length)
        _FRAMEDIRTY = True

    End Sub

    '******************************************************************************************************
    '*** VDP - End of frame                                                                             ***
    '******************************************************************************************************

    ' At the end of the active display area the VDP sets F, the finished frame is handed over for output

    Private Sub FRAME_END()

        _STATEMU = _STATEMU Or &H80

        Array.Copy(_FRAMEW, _FRAME, _FRAME.Length)
        _FRAMEDIRTY = True

    End Sub

    '******************************************************************************************************
    '*** VDP - Time mark of the sender                                                                  ***
    '******************************************************************************************************

    ' T24 is the sender's µs counter (24 bits, wraps after 16.7 s). All following IO accesses count
    ' as happening at this time. The lines up to this time are processed.

    Public Sub TIMEMARK(ByVal T24 As Integer)

        If _TLAST24 < 0 Then
            _TNOW = T24
        Else
            _TNOW += (T24 - _TLAST24) And &HFFFFFF
        End If

        _TLAST24 = T24
        _TMODE = True

        ADVANCE(_TNOW)

    End Sub

    '******************************************************************************************************
    '*** VDP - Process lines and frame ends up to time T                                                ***
    '******************************************************************************************************

    ' Display line Y starts NLINES - 192 + Y lines after the last end of frame; the VDP already checks
    ' its sprites in the line before, so line Y is processed one line before it starts.
    ' The next end of frame follows NLINES lines after the last one. Frame ends continue without
    ' status reads; if F stays set, the VDP suppresses 5S in the following frame. If several frames
    ' without IO access lie in between, VRAM is unchanged and the frames in between are
    ' skipped.

    Private Sub ADVANCE(ByVal T As Long)

        If Not _PHASE Then Exit Sub

        Dim LINE As Double = _TFRAME / _NLINES

        Do

            Dim TA As Double = _TVPREV + (_NLINES - SCR_H) * LINE

            While _NEXTLINE <= SCR_H AndAlso T >= TA + (_NEXTLINE - 1) * LINE
                PROCESS_LINE(_NEXTLINE)
                _NEXTLINE += 1
            End While

            Dim TV As Double = _TVPREV + _TFRAME
            If T < TV Then Exit Do

            While _NEXTLINE <= SCR_H
                PROCESS_LINE(_NEXTLINE)
                _NEXTLINE += 1
            End While

            FRAME_END()
            _FRAMES += 1
            _TVPREV = TV
            _NEXTLINE = 0

            RaiseEvent VDP_Field(1)

            If T >= _TVPREV + 2 * _TFRAME Then
                Dim SKIP As Long = CLng(Math.Floor((T - _TVPREV) / _TFRAME)) - 1
                _TVPREV += SKIP * _TFRAME
                _FRAMES += SKIP
                RaiseEvent VDP_Field(SKIP)
            End If

        Loop

    End Sub

    '******************************************************************************************************
    '*** VDP - Frame position from the VBLANK reads                                                     ***
    '******************************************************************************************************

    ' The VDP's frame period is fixed, only the crystals of VDP and sender differ slightly. A read
    ' with the F bit never comes before the end of frame, but may come arbitrarily late (disabled
    ' interrupts, drivers in H.KEYI). D is the distance of a read from the last end of frame within
    ' one frame.
    ' - Acquisition: the most frequent delay (classes of one line) of the first PHASE_ACQ reads
    '   fixes the frame position; nothing is compared until then.
    ' - Tracking: a read at most PHASE_TOL lines before the expected end of frame pulls the position
    '   forward; a read with the usual delay (below one line) moves it back by at most
    '   PHASE_CREEP_US; delayed reads do not change the position. After PHASE_LOST delayed reads in
    '   a row the position is acquired again.
    ' - Period: from two reads with the usual delay at least PERIOD_FRAMES frames apart, provided
    '   the deviation from the nominal period is below PERIOD_PPM.

    Private Const PHASE_ACQ As Integer = 32
    Private Const PHASE_TOL As Double = 2
    Private Const PHASE_CREEP_US As Double = 0.5
    Private Const PHASE_LOST As Integer = 300
    Private Const PERIOD_FRAMES As Integer = 250
    Private Const PERIOD_PPM As Double = 300

    Private _TFRAME As Double
    Private _LOCKED As Boolean
    Private _PHASED As New List(Of Double)
    Private _DELAYED As Integer
    Private _TREF As Double = -1

    ' First VBLANK reads: PAL/NTSC (313 or 262 lines) from the intervals of the first PERIOD_ACQ
    ' reads. For each period it counts how many intervals are an integer multiple to within one
    ' line; single delayed reads (e.g. while booting) do not disturb.

    Private Const PERIOD_ACQ As Integer = 16
    Private _FTIMES As New List(Of Long)

    Private Sub PHASE_START()

        _FTIMES.Add(_TNOW)
        If _FTIMES.Count < PERIOD_ACQ Then Exit Sub

        Dim HITS(1) As Integer
        Dim LINES() As Integer = {313, 262}

        For C As Integer = 0 To 1

            Dim TC As Double = LINES(C) * LINE_US

            For I As Integer = 1 To _FTIMES.Count - 1
                Dim D As Double = _FTIMES(I) - _FTIMES(I - 1)
                Dim K As Double = Math.Round(D / TC)
                If K >= 1 AndAlso Math.Abs(D - K * TC) < LINE_US Then HITS(C) += 1
            Next

        Next

        If HITS(0) = HITS(1) Or Math.Max(HITS(0), HITS(1)) < (PERIOD_ACQ - 1) \ 2 Then
            _FTIMES.RemoveAt(0)
            Exit Sub
        End If

        _NLINES = If(HITS(0) > HITS(1), 313, 262)
        _FTIMES.Clear()

        _TFRAME = _NLINES * LINE_US
        _PHASE = True
        _TVPREV = _TNOW
        _NEXTLINE = 0
        PHASE_ACQUIRE()

    End Sub

    Private Sub PHASE_ACQUIRE()

        _LOCKED = False
        _PHASED.Clear()
        _DELAYED = 0
        _TREF = -1

    End Sub

    ' Read with the F bit and known frame position

    Private Sub PHASE_FREAD()

        Dim LINE As Double = _TFRAME / _NLINES
        Dim D As Double = (_TNOW - _TVPREV) Mod _TFRAME
        If D < 0 Then D += _TFRAME

        If Not _LOCKED Then

            ' Acquisition: most frequent delay class, within it the smallest delay

            _PHASED.Add(D)
            If _PHASED.Count < PHASE_ACQ Then Exit Sub

            Dim BEST As Integer = _PHASED.GroupBy(Function(V) CInt(Math.Floor(V / LINE))).
                                          OrderByDescending(Function(G) G.Count()).First().Key
            Dim M As Double = _PHASED.Where(Function(V) CInt(Math.Floor(V / LINE)) = BEST).Min()

            _TVPREV += M
            If M > LINE Then _PHASEFIX += 1
            _PHASED.Clear()
            _LOCKED = True
            Exit Sub

        End If

        If D > _TFRAME - PHASE_TOL * LINE Then

            ' End of frame earlier than expected: complete it now

            If _TNOW >= _TVPREV Then

                While _NEXTLINE <= SCR_H
                    PROCESS_LINE(_NEXTLINE)
                    _NEXTLINE += 1
                End While

                FRAME_END()
                _FRAMES += 1
                _NEXTLINE = 0

            End If

            If _TFRAME - D > LINE Then _PHASEFIX += 1

            _TVPREV = _TNOW
            _DELAYED = 0
            PERIOD_UPDATE()

        ElseIf D < LINE Then

            ' Usual delay

            _TVPREV += Math.Min(D, PHASE_CREEP_US)
            _DELAYED = 0
            PERIOD_UPDATE()

        Else

            ' Delayed read

            _DELAYED += 1
            If _DELAYED > PHASE_LOST Then PHASE_ACQUIRE()

        End If

    End Sub

    ' Track the period from reads with the usual delay

    Private Sub PERIOD_UPDATE()

        If _TREF < 0 Then
            _TREF = _TNOW
            Exit Sub
        End If

        Dim K As Long = CLng(Math.Round((_TNOW - _TREF) / _TFRAME))
        If K < PERIOD_FRAMES Then Exit Sub

        Dim TN As Double = (_TNOW - _TREF) / K
        Dim NOMINAL As Double = _NLINES * LINE_US

        If Math.Abs(TN - NOMINAL) < NOMINAL * PERIOD_PPM / 1000000.0 Then _TFRAME = TN

        _TREF = _TNOW

    End Sub

    '******************************************************************************************************
    '*** VDP - Compare a status read                                                                    ***
    '******************************************************************************************************

    ' In text mode and with the display blanked the VDP does not evaluate sprites; it then returns 5S
    ' and bits 4-0 with an undefined internal value that stays until the next read. If that was the
    ' case since the last read, only F and C are compared.
    ' Without 5S, bits 4-0 show the entry the sprite check of the current line had reached at the
    ' time of the read; F, 5S and C are compared then.

    Private Sub STATUS_COMPARE(ByVal IODATA As Integer, ByVal INFO As String)

        Dim MASK As Integer = &HFF

        If _STATUNDEF Then
            MASK = &HA0
        ElseIf ((IODATA Or _STATEMU) And &H40) = 0 Then
            MASK = &HE0
        End If

        _STATCOMPARED += 1

        If (IODATA And MASK) <> (_STATEMU And MASK) Then

            _STATMISMATCHES += 1

            If _STATLOG.Count < _STATLOGMAX Then
                _STATLOG.Add(UI(" Status frame ", " Status Frame ") & _FRAMES & UI(" exp &H", " erw &H") & _STATEMU.ToString("X2") & UI(" real &H", " echt &H") & IODATA.ToString("X2") & INFO &
                             If((_STATEMU And &H40) <> 0 AndAlso (IODATA And &H40) = 0, UI(" (5S expected only)", " (5S nur erwartet)"), ""))
            End If

        End If

    End Sub

    '******************************************************************************************************
    '*** VDP - Background of a display line                                                             ***
    '******************************************************************************************************

    ' Mode: bit 0 = M1 (text), bit 1 = M2 (multicolor), bit 2 = M3 (Graphics II)
    ' With M3 set, R3 bit 7 and R4 bit 2 select the base addresses, the remaining bits of R3 and R4
    ' are masks over the character number (0-767) of the three screen thirds.

    ' Text modes: 240 pixels active, left border 19 instead of 13 clocks (data manual table 3-3), the
    ' text area therefore starts 6 pixels after the start of the 256 pixel wide graphics area

    Private Const TEXT_LEFT As Integer = 6

    Private Sub RENDER_LINE(ByVal Y As Integer)

        Dim M As Integer = _MODE
        Dim R3 As Integer = _REGISTER(3)
        Dim R4 As Integer = _REGISTER(4)
        Dim TC As Integer = (_REGISTER(7) >> 4) And &HF
        Dim BD As Integer = _REGISTER(7) And &HF
        Dim NT As Integer = (_REGISTER(2) And &HF) << 10
        Dim ROW As Integer = Y >> 3
        Dim LN As Integer = Y And 7
        Dim THIRD As Integer = (Y >> 6) << 8
        Dim PG, CT, PATMASK, COLMASK As Integer

        If (M And 4) <> 0 Then

            PG = (R4 And &H4) << 11
            CT = (R3 And &H80) << 6
            PATMASK = ((R4 And &H3) << 8) Or &HFF
            COLMASK = ((R3 And &H7F) << 3) Or &H7

        Else

            PG = (R4 And &H7) << 11
            CT = R3 << 6

        End If

        Select Case M

            Case 1, 5

                ' Text (40 x 6 pixels), with M3 set using three pattern tables

                For X As Integer = 0 To TEXT_LEFT - 1
                    _LINE(X) = BD
                Next

                For X As Integer = TEXT_LEFT + 240 To SCR_W - 1
                    _LINE(X) = BD
                Next

                For COL As Integer = 0 To 39
                    Dim NAME As Integer = VR(NT + ROW * 40 + COL)
                    If M = 5 Then NAME = (THIRD Or NAME) And PATMASK
                    Dim PAT As Integer = VR(PG + NAME * 8 + LN)
                    Dim XP As Integer = TEXT_LEFT + COL * 6
                    For B As Integer = 0 To 5
                        _LINE(XP + B) = If((PAT And (&H80 >> B)) <> 0, TC, BD)
                    Next
                Next

            Case 3, 7

                ' Text + multicolor: 40 columns of 4 pixels text colour and 2 pixels backdrop

                For X As Integer = 0 To TEXT_LEFT - 1
                    _LINE(X) = BD
                Next

                For X As Integer = TEXT_LEFT + 240 To SCR_W - 1
                    _LINE(X) = BD
                Next

                For COL As Integer = 0 To 39
                    Dim XP As Integer = TEXT_LEFT + COL * 6
                    For B As Integer = 0 To 5
                        _LINE(XP + B) = If(B < 4, TC, BD)
                    Next
                Next

            Case 0, 4

                ' Graphics I / Graphics II (32 x 8 pixels)

                For COL As Integer = 0 To 31
                    Dim NAME As Integer = VR(NT + ROW * 32 + COL)
                    Dim PAT, COLOR As Integer
                    If M = 4 Then
                        Dim CH As Integer = THIRD Or NAME
                        PAT = VR(PG + (CH And PATMASK) * 8 + LN)
                        COLOR = VR(CT + (CH And COLMASK) * 8 + LN)
                    Else
                        PAT = VR(PG + NAME * 8 + LN)
                        COLOR = VR(CT + (NAME >> 3))
                    End If
                    Dim FG As Integer = (COLOR >> 4) And &HF
                    Dim BG As Integer = COLOR And &HF
                    Dim XP As Integer = COL * 8
                    For B As Integer = 0 To 7
                        _LINE(XP + B) = If((PAT And (&H80 >> B)) <> 0, FG, BG)
                    Next
                Next

            Case 2, 6

                ' Multicolor (4 x 4 pixel blocks, upper nibble left), with M3 set using three
                ' pattern tables

                For COL As Integer = 0 To 31
                    Dim NAME As Integer = VR(NT + ROW * 32 + COL)
                    If M = 6 Then NAME = (THIRD Or NAME) And PATMASK
                    Dim COLOR As Integer = VR(PG + NAME * 8 + (ROW And 3) * 2 + ((Y >> 2) And 1))
                    Dim XP As Integer = COL * 8
                    For B As Integer = 0 To 3
                        _LINE(XP + B) = (COLOR >> 4) And &HF
                        _LINE(XP + 4 + B) = COLOR And &HF
                    Next
                Next

        End Select

    End Sub

    '******************************************************************************************************
    '*** VDP - Sprites of a display line                                                                ***
    '******************************************************************************************************

    ' Y = -1 (FFh) is the top display line, the sprite line is computed 8-bit wise as
    ' (display line - 1 - Y) AND FFh. D0h terminates the list.
    ' At most 4 sprites per line, the sprite number sets the priority (0 = highest).
    ' EC (colour bit 7) shifts 32 pixels to the left, colour 0 is transparent.
    ' Status capture: the first 5th sprite (earliest line) and collisions between the visible
    ' sprites of a line, only within x = 0..255, also with colour 0.

    Private Sub RENDER_SPRITES(ByVal Y As Integer, ByVal DRAW As Boolean)

        Dim SA As Integer = (_REGISTER(5) And &H7F) << 7
        Dim SG As Integer = (_REGISTER(6) And &H7) << 11
        Dim SIZE16 As Boolean = (_REGISTER(1) And &H2) <> 0
        Dim MAG As Integer = If((_REGISTER(1) And &H1) <> 0, 2, 1)
        Dim SIZE As Integer = If(SIZE16, 16, 8)
        Dim H As Integer = SIZE * MAG
        Dim COUNT As Integer = 0

        For X As Integer = 0 To SCR_W - 1
            _SPRCOLOR(X) = -1
            _SPRHIT(X) = False
        Next

        For I As Integer = 0 To 31

            Dim A As Integer = SA + I * 4
            Dim SY As Integer = VR(A)

            If SY = &HD0 Then Exit For

            Dim DY As Integer = (Y - 1 - SY) And &HFF
            If DY >= H Then Continue For

            COUNT += 1
            If COUNT > 4 Then
                If _FRM5S < 0 Then _FRM5S = I
                Exit For
            End If

            Dim NAME As Integer = VR(A + 2)
            If SIZE16 Then NAME = NAME And &HFC

            Dim P As Integer = SG + NAME * 8 + DY \ MAG
            Dim BITS As Integer = VR(P) << 8
            If SIZE16 Then BITS = BITS Or VR(P + 16)

            Dim X0 As Integer = VR(A + 1)
            Dim C As Integer = VR(A + 3)
            If (C And &H80) <> 0 Then X0 -= 32
            C = C And &HF

            For PX As Integer = 0 To H - 1

                If (BITS And (&H8000 >> (PX \ MAG))) = 0 Then Continue For

                Dim X As Integer = X0 + PX
                If X < 0 Or X >= SCR_W Then Continue For

                If _SPRHIT(X) Then _FRMCOLL = True Else _SPRHIT(X) = True

                If C <> 0 AndAlso _SPRCOLOR(X) < 0 Then _SPRCOLOR(X) = C

            Next

        Next

        If Not DRAW Then Exit Sub

        For X As Integer = 0 To SCR_W - 1
            If _SPRCOLOR(X) >= 0 Then _LINE(X) = _SPRCOLOR(X)
        Next

    End Sub

    '******************************************************************************************************
    '*** VDP - Output the frame                                                                         ***
    '******************************************************************************************************

    ' Copies the last built frame magnified into the output image and signals it.
    ' Call from the UI thread only.

    Public Event VDP_Out(ByVal PLANE As Bitmap)

    '******************************************************************************************************
    '*** VDP - Fields for the video recording                                                           ***
    '******************************************************************************************************

    ' Signalled at every end of frame with the number of fields the finished frame stands for
    ' (more than 1 for skipped frames without IO access, VRAM is unchanged in them)

    Public Event VDP_Field(ByVal COUNT As Long)

    ' The last finished frame as 0x00RRGGBB pixels, magnified, with a border of BORDER pixels in the
    ' backdrop colour; PIXELS holds (SCR_W * ZOOM + 2 * BORDER) x (SCR_H * ZOOM + 2 * BORDER) pixels

    Public Sub VIDEOFRAME(ByVal PIXELS() As Integer, ByVal BORDER As Integer)

        Dim W As Integer = SCR_W * ZOOM + 2 * BORDER
        Dim H As Integer = SCR_H * ZOOM + 2 * BORDER
        Dim BD As Integer = _ARGB(_COLORBACK) And &HFFFFFF

        For Y As Integer = 0 To H - 1

            Dim O As Integer = Y * W
            Dim FY As Integer = (Y - BORDER) \ ZOOM

            If Y < BORDER OrElse FY >= SCR_H Then
                For X As Integer = 0 To W - 1
                    PIXELS(O + X) = BD
                Next
                Continue For
            End If

            For X As Integer = 0 To BORDER - 1
                PIXELS(O + X) = BD
                PIXELS(O + W - 1 - X) = BD
            Next

            Dim F As Integer = FY * SCR_W

            For X As Integer = 0 To SCR_W * ZOOM - 1
                PIXELS(O + BORDER + X) = _ARGB(_FRAME(F + X \ ZOOM)) And &HFFFFFF
            Next

        Next

    End Sub

    Public Sub PRESENT()

        If Not _FRAMEDIRTY Then Exit Sub

        _FRAMEDIRTY = False

        Dim DATA As BitmapData = _IMAGE.LockBits(New Rectangle(0, 0, SCR_W * ZOOM, SCR_H * ZOOM),
                                                 ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb)

        Try

            For Y As Integer = 0 To SCR_H - 1

                Dim O As Integer = Y * SCR_W

                For X As Integer = 0 To SCR_W - 1
                    Dim ARGB As Integer = _ARGB(_FRAME(O + X))
                    Dim XP As Integer = X * ZOOM
                    For Z As Integer = 0 To ZOOM - 1
                        _ROWBUF(XP + Z) = ARGB
                    Next
                Next

                For Z As Integer = 0 To ZOOM - 1
                    Marshal.Copy(_ROWBUF, 0, IntPtr.Add(DATA.Scan0, (Y * ZOOM + Z) * DATA.Stride), SCR_W * ZOOM)
                Next

            Next

        Finally

            _IMAGE.UnlockBits(DATA)

        End Try

        RaiseEvent VDP_Out(_IMAGE)

    End Sub
End Class
