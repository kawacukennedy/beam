#include "SettingsPage.xaml.h"
#include "../../settings/settings.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::BlueBeam::implementation
{
    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        LoadSettings();
    }

    void SettingsPage::LoadSettings()
    {
        Settings settings;
        UserNameBox().Text(winrt::to_hstring(settings.get_user_name()));
        ThemeCombo().SelectedIndex(settings.get_theme() == "dark" ? 1 : (settings.get_theme() == "high_contrast" ? 2 : 0));
        DownloadPathBox().Text(winrt::to_hstring(settings.get_download_path()));
        EncryptionToggle().IsOn(settings.get_encryption_enabled());
        AutoLockBox().Text(winrt::to_hstring(std::to_string(settings.get_auto_lock_timeout())));
        BiometricToggle().IsOn(settings.get_biometric_auth_enabled());
        TwoFactorToggle().IsOn(settings.get_two_factor_enabled());
        LanguageBox().Text(winrt::to_hstring(settings.get_language()));
        NotificationsToggle().IsOn(settings.get_notifications_enabled());
        AutoUpdateToggle().IsOn(settings.get_auto_update_enabled());
        ProfilePicBox().Text(winrt::to_hstring(settings.get_profile_picture_path()));
        EmailBox().Text(winrt::to_hstring(settings.get_email()));
    }

    void SettingsPage::SaveButton_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        Settings settings;
        settings.set_user_name(winrt::to_string(UserNameBox().Text()));
        std::string theme = ThemeCombo().SelectedIndex() == 1 ? "dark" : (ThemeCombo().SelectedIndex() == 2 ? "high_contrast" : "light");
        settings.set_theme(theme);
        settings.set_download_path(winrt::to_string(DownloadPathBox().Text()));
        settings.set_encryption_enabled(EncryptionToggle().IsOn());
        settings.set_auto_lock_timeout(std::stoi(winrt::to_string(AutoLockBox().Text())));
        settings.set_biometric_auth_enabled(BiometricToggle().IsOn());
        settings.set_two_factor_enabled(TwoFactorToggle().IsOn());
        settings.set_language(winrt::to_string(LanguageBox().Text()));
        settings.set_notifications_enabled(NotificationsToggle().IsOn());
        settings.set_auto_update_enabled(AutoUpdateToggle().IsOn());
        settings.set_profile_picture_path(winrt::to_string(ProfilePicBox().Text()));
        settings.set_email(winrt::to_string(EmailBox().Text()));
        settings.save();
    }
}