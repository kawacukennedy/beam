import React, { useState } from 'react';
import { useSelector, useDispatch } from 'react-redux';
import { setTheme, setNotifications, setAutoAcceptFiles } from '../slices/settingsSlice';

const Settings = () => {
  const settings = useSelector((state) => state.settings);
  const dispatch = useDispatch();
  const [activeTab, setActiveTab] = useState('profile');

  const tabs = [
    { id: 'profile', label: 'Profile' },
    { id: 'preferences', label: 'Preferences' },
    { id: 'storage', label: 'Storage' },
    { id: 'security', label: 'Security' },
    { id: 'about', label: 'About' },
  ];

  return (
    <div className="p-6 max-w-4xl mx-auto">
      <h1 className="text-2xl font-bold mb-6">Settings</h1>

      <div className="flex border-b border-gray-200 dark:border-gray-700 mb-6">
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`px-4 py-2 font-medium ${
              activeTab === tab.id
                ? 'border-b-2 border-primary-light dark:border-primary-dark text-primary-light dark:text-primary-dark'
                : 'text-gray-600 dark:text-gray-400'
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      <div className="space-y-6">
        {activeTab === 'profile' && (
          <div>
            <h2 className="text-xl font-semibold mb-4">Profile</h2>
            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium mb-1">Username</label>
                <input
                  type="text"
                  className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-slate-800"
                  placeholder="Enter your username"
                />
              </div>
              <div>
                <label className="block text-sm font-medium mb-1">Device ID</label>
                <p className="text-sm text-gray-600 dark:text-gray-400">ABC123-DEF456</p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'preferences' && (
          <div>
            <h2 className="text-xl font-semibold mb-4">Preferences</h2>
            <div className="space-y-4">
              <div className="flex items-center justify-between">
                <span>Dark Mode</span>
                <button
                  onClick={() => dispatch(setTheme(settings.theme === 'dark' ? 'light' : 'dark'))}
                  className={`w-12 h-6 rounded-full p-1 transition-colors ${
                    settings.theme === 'dark' ? 'bg-primary-dark' : 'bg-gray-300'
                  }`}
                >
                  <div
                    className={`w-4 h-4 bg-white rounded-full transition-transform ${
                      settings.theme === 'dark' ? 'translate-x-6' : 'translate-x-0'
                    }`}
                  />
                </button>
              </div>
              <div className="flex items-center justify-between">
                <span>Notifications</span>
                <input
                  type="checkbox"
                  checked={settings.notifications}
                  onChange={(e) => dispatch(setNotifications(e.target.checked))}
                  className="w-4 h-4"
                />
              </div>
              <div className="flex items-center justify-between">
                <span>Auto-accept files from trusted devices</span>
                <input
                  type="checkbox"
                  checked={settings.autoAcceptFiles}
                  onChange={(e) => dispatch(setAutoAcceptFiles(e.target.checked))}
                  className="w-4 h-4"
                />
              </div>
            </div>
          </div>
        )}

        {activeTab === 'security' && (
          <div>
            <h2 className="text-h2 font-semibold mb-4">Security</h2>
            <div className="space-y-4">
              <div>
                <h3 className="font-medium mb-2">Trusted Devices</h3>
                <p className="text-body text-neutral">Manage devices you trust for auto-accept file transfers.</p>
                <div className="mt-2 space-y-2">
                  {/* TODO: List trusted devices */}
                  <p className="text-caption text-gray-600 dark:text-gray-400">No trusted devices yet.</p>
                </div>
              </div>
              <div>
                <h3 className="font-medium mb-2">Encryption Keys</h3>
                <button className="bg-error hover:bg-red-600 text-white px-4 py-2 rounded-lg transition-colors">
                  Revoke All Keys
                </button>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'about' && (
          <div>
            <h2 className="text-h2 font-semibold mb-4">About</h2>
            <div className="space-y-4">
              <div>
                <p className="text-body"><strong>BlueBeam</strong> v1.0.0</p>
                <p className="text-caption text-neutral">Offline. Private. Instant Bluetooth Communication.</p>
              </div>
              <div>
                <p className="text-body">© 2024 BlueBeam. All rights reserved.</p>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'storage' && (
          <div>
            <h2 className="text-xl font-semibold mb-4">Storage</h2>
            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium mb-1">Default Download Folder</label>
                <input
                  type="text"
                  className="w-full px-3 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-slate-800"
                  placeholder="/Users/username/Downloads"
                />
              </div>
              <button className="bg-blue-500 text-white px-4 py-2 rounded-lg">
                Clear Cache
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default Settings;