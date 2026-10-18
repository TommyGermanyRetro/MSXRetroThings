using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Microsoft.Win32;

internal static class Installer
{
    private const string AppName = "SVI-318 Emulator";
    private const string RegistrySubKey = @"Software\Microsoft\Windows\CurrentVersion\Uninstall\SVI318";

    [STAThread]
    private static int Main(string[] args)
    {
        if (args.Length > 0 && String.Equals(args[0], "/uninstall", StringComparison.OrdinalIgnoreCase))
        {
            return Uninstall();
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new InstallerForm());
        return 0;
    }

    private static int Uninstall()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        string installDirectory = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        DialogResult answer = MessageBox.Show(
            "Remove the SVI-318 Emulator and its settings?",
            AppName + " Uninstall",
            MessageBoxButtons.YesNo,
            MessageBoxIcon.Question);
        if (answer != DialogResult.Yes) return 0;

        string startMenuShortcut = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.StartMenu),
            "Programs",
            AppName + ".lnk");
        string desktopShortcut = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            AppName + ".lnk");
        try
        {
            if (File.Exists(startMenuShortcut)) File.Delete(startMenuShortcut);
            if (File.Exists(desktopShortcut)) File.Delete(desktopShortcut);
            Registry.CurrentUser.DeleteSubKeyTree(RegistrySubKey, false);

            ProcessStartInfo cleanup = new ProcessStartInfo
            {
                FileName = Environment.GetEnvironmentVariable("ComSpec"),
                Arguments = "/C timeout /T 2 /NOBREAK >nul & rmdir /S /Q \"" + installDirectory + "\"",
                CreateNoWindow = true,
                UseShellExecute = false,
                WindowStyle = ProcessWindowStyle.Hidden
            };
            Process.Start(cleanup);
            return 0;
        }
        catch (Exception error)
        {
            MessageBox.Show(
                "Uninstalling failed:\r\n" + error.Message,
                AppName,
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
    }

    private sealed class InstallerForm : Form
    {
        private readonly TextBox pathBox;
        private readonly Button installButton;
        private readonly Label statusLabel;

        public InstallerForm()
        {
            Text = AppName + " Setup";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new System.Drawing.Size(520, 205);

            Controls.Add(new Label
            {
                Left = 18,
                Top = 18,
                Width = 484,
                Height = 38,
                Text = "Choose the installation folder. The setup installs the emulator, its manual and, if included, its ROM images."
            });

            pathBox = new TextBox
            {
                Left = 18,
                Top = 69,
                Width = 385,
                Text = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "Programs",
                    "SVI318")
            };
            Controls.Add(pathBox);

            Button browseButton = new Button
            {
                Left = 412,
                Top = 67,
                Width = 90,
                Text = "Browse..."
            };
            browseButton.Click += Browse;
            Controls.Add(browseButton);

            statusLabel = new Label
            {
                Left = 18,
                Top = 112,
                Width = 484,
                Height = 32,
                Text = "Start menu and desktop shortcuts and an uninstall entry are created."
            };
            Controls.Add(statusLabel);

            installButton = new Button
            {
                Left = 322,
                Top = 158,
                Width = 90,
                Text = "Install"
            };
            installButton.Click += Install;
            Controls.Add(installButton);

            Button cancelButton = new Button
            {
                Left = 412,
                Top = 158,
                Width = 90,
                Text = "Cancel",
                DialogResult = DialogResult.Cancel
            };
            Controls.Add(cancelButton);
            AcceptButton = installButton;
            CancelButton = cancelButton;
        }

        private void Browse(object sender, EventArgs e)
        {
            using (FolderBrowserDialog dialog = new FolderBrowserDialog())
            {
                dialog.Description = "Installation folder for the SVI-318 Emulator";
                dialog.ShowNewFolderButton = true;
                DirectoryInfo selected = new DirectoryInfo(pathBox.Text);
                while (selected != null && !selected.Exists) selected = selected.Parent;
                if (selected != null) dialog.SelectedPath = selected.FullName;
                if (dialog.ShowDialog(this) == DialogResult.OK) pathBox.Text = dialog.SelectedPath;
            }
        }

        private void Install(object sender, EventArgs e)
        {
            string installDirectory = pathBox.Text.Trim();
            if (installDirectory.Length == 0)
            {
                MessageBox.Show(this, "Please choose an installation folder.",
                    AppName, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            installButton.Enabled = false;
            statusLabel.Text = "Installing...";
            try
            {
                Directory.CreateDirectory(installDirectory);
                ExtractResource("svi318.exe", Path.Combine(installDirectory, "svi318.exe"));
                ExtractResource("MANUAL.md", Path.Combine(installDirectory, "MANUAL.md"));
                string userDataDirectory = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "SVI-318");
                foreach (string folder in new[] { "CAS", "DSK", Path.Combine("DSK", "Original"), "HDD", "MOD", "PRINT", "STATE", "ROM", "VIDEO" })
                {
                    Directory.CreateDirectory(Path.Combine(userDataDirectory, folder));
                }
                ExtractBlankDisks(Path.Combine(userDataDirectory, "DSK", "Original"));
                foreach (string rom in new[] { "svi111.rom", "svi806.rom", "svi609.rom" })
                {
                    ExtractOptionalResource(rom, Path.Combine(installDirectory, rom));
                }
                using (FileStream currentSetup = File.OpenRead(Assembly.GetExecutingAssembly().Location))
                using (FileStream uninstaller = File.Create(Path.Combine(installDirectory, "uninstall.exe")))
                {
                    currentSetup.CopyTo(uninstaller);
                }

                CreateShortcuts(installDirectory);
                RegisterUninstaller(installDirectory);
                MessageBox.Show(this,
                    "The SVI-318 Emulator has been installed.\r\n\r\nInstallation folder:\r\n" + installDirectory,
                    "Installation complete",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                Close();
            }
            catch (Exception error)
            {
                statusLabel.Text = "Installation failed.";
                installButton.Enabled = true;
                MessageBox.Show(this,
                    "The installation failed:\r\n" + error.Message,
                    AppName,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        // ROM images are part of the setup only when it was built with them; otherwise the user places
        // them in the ROM folder of the documents.
        private static void ExtractOptionalResource(string name, string destination)
        {
            if (Assembly.GetExecutingAssembly().GetManifestResourceInfo(name) == null) return;
            ExtractResource(name, destination);
        }

        // Blank disks go to the Original folder of the disk media folder, write-protected; a disk that is
        // already there stays as it is.
        private static void ExtractBlankDisks(string folder)
        {
            const string prefix = "disk.";
            foreach (string name in Assembly.GetExecutingAssembly().GetManifestResourceNames())
            {
                if (!name.StartsWith(prefix, StringComparison.Ordinal)) continue;
                string destination = Path.Combine(folder, name.Substring(prefix.Length));
                if (File.Exists(destination)) continue;
                ExtractResource(name, destination);
                File.SetAttributes(destination, File.GetAttributes(destination) | FileAttributes.ReadOnly);
            }
        }

        private static void ExtractResource(string name, string destination)
        {
            using (Stream resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(name))
            {
                if (resource == null) throw new InvalidDataException("Setup file missing: " + name);
                using (FileStream output = File.Create(destination)) resource.CopyTo(output);
            }
        }

        private static void CreateShortcuts(string installDirectory)
        {
            string startMenuDirectory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.StartMenu), "Programs");
            string desktopDirectory = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            Directory.CreateDirectory(startMenuDirectory);
            Directory.CreateDirectory(desktopDirectory);
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null) throw new InvalidOperationException("Windows Script Host is not available.");
            object shell = Activator.CreateInstance(shellType);
            try
            {
                CreateShortcut(shellType, shell, Path.Combine(startMenuDirectory, AppName + ".lnk"), installDirectory);
                CreateShortcut(shellType, shell, Path.Combine(desktopDirectory, AppName + ".lnk"), installDirectory);
            }
            finally
            {
                if (Marshal.IsComObject(shell)) Marshal.ReleaseComObject(shell);
            }
        }

        private static void CreateShortcut(Type shellType, object shell, string path, string installDirectory)
        {
            object shortcut = null;
            try
            {
                shortcut = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod,
                    null, shell, new object[] { path });
                Type shortcutType = shortcut.GetType();
                shortcutType.InvokeMember("TargetPath", BindingFlags.SetProperty,
                    null, shortcut, new object[] { Path.Combine(installDirectory, "svi318.exe") });
                shortcutType.InvokeMember("WorkingDirectory", BindingFlags.SetProperty,
                    null, shortcut, new object[] { installDirectory });
                shortcutType.InvokeMember("Description", BindingFlags.SetProperty,
                    null, shortcut, new object[] { AppName });
                shortcutType.InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
            }
            finally
            {
                if (shortcut != null && Marshal.IsComObject(shortcut)) Marshal.ReleaseComObject(shortcut);
            }
        }

        private static void RegisterUninstaller(string installDirectory)
        {
            using (RegistryKey key = Registry.CurrentUser.CreateSubKey(RegistrySubKey))
            {
                if (key == null) throw new InvalidOperationException("The uninstall entry could not be created.");
                string uninstaller = Path.Combine(installDirectory, "uninstall.exe");
                key.SetValue("DisplayName", AppName);
                key.SetValue("DisplayVersion", "1.0");
                key.SetValue("Publisher", "MSX project");
                key.SetValue("InstallLocation", installDirectory);
                key.SetValue("DisplayIcon", Path.Combine(installDirectory, "svi318.exe"));
                key.SetValue("UninstallString", "\"" + uninstaller + "\" /uninstall");
            }
        }
    }
}
