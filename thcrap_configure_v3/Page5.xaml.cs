using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page5.xaml
    /// </summary>
    public partial class Page5 : UserControl
    {
        bool? isUTLPresent = null;
        GlobalConfig config = null;

        public Page5()
        {
            InitializeComponent();
        }

        public void Enter()
        {
            warningImage.Source = Imaging.CreateBitmapSourceFromHIcon(
                SystemIcons.Warning.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());

            if (File.Exists("bin\\Universal THCRAP Launcher.exe"))
                warningImage.Visibility = Visibility.Collapsed;

            if (config == null)
            {
                config = new GlobalConfig();
                GlobalConfig.ShortcutDestinations dest = config.default_shortcut_destinations;

                checkboxDesktop.IsChecked      = dest.HasFlag(GlobalConfig.ShortcutDestinations.Desktop);
                checkboxStartMenu.IsChecked    = dest.HasFlag(GlobalConfig.ShortcutDestinations.StartMenu);
                checkboxGamesFolder.IsChecked  = dest.HasFlag(GlobalConfig.ShortcutDestinations.GamesFolder);
                checkboxThcrapFolder.IsChecked = dest.HasFlag(GlobalConfig.ShortcutDestinations.ThcrapFolder);
            }
        }

        private void checkbox_Checked(object sender, RoutedEventArgs e)
        {
            if (warningPanel == null)
                return;

            if (isUTLPresent == null)
                isUTLPresent = File.Exists("bin\\Universal THCRAP Launcher.exe");

            if (checkboxDesktop.IsChecked == true || checkboxStartMenu.IsChecked == true ||
                checkboxGamesFolder.IsChecked == true || checkboxThcrapFolder.IsChecked == true || isUTLPresent == true)
                warningPanel.Visibility = Visibility.Hidden;
            else
                warningPanel.Visibility = Visibility.Visible;
        }

        private void NoShortcutsMoreDetails(object sender, MouseButtonEventArgs e)
        {
            MessageBox.Show("With the way thcrap works, your game files are never modified, and running the games after setup program will still run the original, unpatched games.\n\n" +
                            "If you want something more like traditional patches with a patched exe aside the original one in the game folder, choose to create the shortcuts \"in the games' folder\".",
                            "More details regarding launching Touhou games with thcrap", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void CreateShortcuts(string configName, IEnumerable<ThcrapDll.games_js_entry> games, ThcrapDll.ShortcutsDestination destination)
        {
            ThcrapDll.games_js_entry[] gamesArray = new ThcrapDll.games_js_entry[games.Count() + 1];
            int i = 0;
            foreach (var it in games)
            {
                gamesArray[i] = it;
                i++;
            }
            gamesArray[i] = new ThcrapDll.games_js_entry();

            ThcrapDll.CreateShortcuts(configName, gamesArray, destination, config.shortcuts_type);
        }

        public void Leave(string configName, IEnumerable<ThcrapDll.games_js_entry> games)
        {
            config.default_shortcut_destinations =
                (checkboxDesktop.IsChecked      == true ? GlobalConfig.ShortcutDestinations.Desktop      : 0) |
                (checkboxStartMenu.IsChecked    == true ? GlobalConfig.ShortcutDestinations.StartMenu    : 0) |
                (checkboxGamesFolder.IsChecked  == true ? GlobalConfig.ShortcutDestinations.GamesFolder  : 0) |
                (checkboxThcrapFolder.IsChecked == true ? GlobalConfig.ShortcutDestinations.ThcrapFolder : 0);
            config.Save();

            if (checkboxDesktop.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_DESKTOP);
            if (checkboxStartMenu.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_START_MENU);
            if (checkboxGamesFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_GAMES_DIRECTORY);
            if (checkboxThcrapFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_THCRAP_DIR);
        }
    }
}
