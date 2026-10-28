Imports System.IO
Imports System.ComponentModel
Imports System.Threading

Public Class FrmInit

    Dim PortNameCOM As String

    ' Runs when the form is closed

    Private Sub FrmInit_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing

        ' Store the settings of the COM connection

        If cmbCOMPort.Text <> "-" Then

            frmMain.COM.PortName = cmbCOMPort.Text
            frmMain.frmMain_ToolStripMenuItem_Verbinden.Enabled = True
            frmMain.frmMain_ToolStripMenuItem_Trennen.Enabled = False
            PortNameCOM = cmbCOMPort.Text

        Else

            frmMain.frmMain_ToolStripMenuItem_Verbinden.Enabled = False
            frmMain.frmMain_ToolStripMenuItem_Trennen.Enabled = False
            frmMain.COM.PortName = "-"
            PortNameCOM = "-"

        End If

        frmMain.COM.Save()

    End Sub

    ' Runs when the form is loaded

    Private Sub FrmInit_Load(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles MyBase.Load

        cmbCOMPort.Items.Clear()

        Dim myPort As Array

        ' Get the available COM port names and fill the drop-down list

        myPort = IO.Ports.SerialPort.GetPortNames()

        cmbCOMPort.Items.Add("-")

        For i = 0 To UBound(myPort)
            cmbCOMPort.Items.Add(myPort(i))
        Next

        If PortNameCOM = "" Then
            cmbCOMPort.Text = frmMain.COM.PortName
            PortNameCOM = frmMain.COM.PortName
        Else
            cmbCOMPort.Text = PortNameCOM
        End If

        lblBaud.Text = frmMain.COM.Client.BaudRate

        ' Texts in the selected language

        Me.Text = UI("MSX - Communication settings", "MSX - Kommunikationsparameter")
        Label12.Text = UI("COM port:", "COM-Port:")
        Label11.Text = UI("Baud rate:", "Baudrate:")
        Label10.Text = UI("Data bits:", "Datenbits:")
        Label4.Text = UI("Parity:", "Parität:")
        Label8.Text = UI("None", "Keine")
        Label5.Text = UI("Stop bits:", "Stoppbits:")

    End Sub

    ' Close button

    Private Sub BtnOk_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnOk.Click
        frmMain.COM.Save()
        Me.Close()
    End Sub

    Private Sub CmbCOMPort_SelectedIndexChanged(sender As Object, e As EventArgs) Handles cmbCOMPort.SelectedIndexChanged
        PortNameCOM = cmbCOMPort.Text
    End Sub

End Class
