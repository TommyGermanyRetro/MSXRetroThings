Public Module UiText

    '******************************************************************************************************
    '*** UI - Language of the user interface                                                            ***
    '******************************************************************************************************

    ' "en" (default) or "de", stored as the second line of the settings file

    Public Language As String = "en"

    ' Every visible text is written as UI(English, German)

    Public Function UI(ByVal EN As String, ByVal DE As String) As String

        If Language = "de" Then Return DE
        Return EN

    End Function

End Module
