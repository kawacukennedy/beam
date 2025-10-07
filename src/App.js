import React, { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import { useSelector } from 'react-redux';
import DeviceDiscovery from './components/DeviceDiscovery';
import ChatWindow from './components/ChatWindow';
import Settings from './components/Settings';
import Onboarding from './components/Onboarding';
import FileTransferModal from './components/FileTransferModal';

function App() {
  const [currentView, setCurrentView] = useState('onboarding');
  const [showOnboarding, setShowOnboarding] = useState(true);
  const [fileTransfer, setFileTransfer] = useState(null);
  const theme = useSelector((state) => state.settings.theme);

  useEffect(() => {
    // Check if onboarding is complete
    const onboardingComplete = localStorage.getItem('bluebeam_onboarding_complete');
    if (onboardingComplete) {
      setShowOnboarding(false);
      setCurrentView('discovery');
    }
  }, []);

  const handleOnboardingComplete = () => {
    localStorage.setItem('bluebeam_onboarding_complete', 'true');
    setShowOnboarding(false);
    setCurrentView('discovery');
  };

  const views = {
    discovery: DeviceDiscovery,
    chat: ChatWindow,
    settings: Settings,
  };

  const CurrentComponent = views[currentView];

  if (showOnboarding) {
    return <Onboarding onComplete={handleOnboardingComplete} />;
  }

  return (
    <div className={`h-screen ${theme === 'dark' ? 'bg-background-dark text-gray-100' : 'bg-background-light text-gray-900'}`}>
      <motion.div
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        transition={{ duration: 0.5 }}
        className="flex h-full"
      >
        {/* Sidebar */}
        <div className={`w-64 ${theme === 'dark' ? 'bg-surface-dark' : 'bg-surface-light'} shadow-lg border-r border-gray-200 dark:border-gray-700`}>
          <div className="p-4 border-b border-gray-200 dark:border-gray-700">
            <h1 className="text-h1 font-bold text-primary-light">BlueBeam</h1>
            <p className="text-caption text-neutral">Offline. Private. Instant.</p>
          </div>
          <nav className="mt-4">
            <button
              onClick={() => setCurrentView('discovery')}
              className={`w-full text-left px-4 py-3 hover:bg-gray-100 dark:hover:bg-surface-dark transition-colors ${
                currentView === 'discovery' ? 'bg-primary-light text-white hover:bg-primary-dark' : 'text-gray-700 dark:text-gray-300'
              }`}
            >
              Device Discovery
            </button>
            <button
              onClick={() => setCurrentView('chat')}
              className={`w-full text-left px-4 py-3 hover:bg-gray-100 dark:hover:bg-surface-dark transition-colors ${
                currentView === 'chat' ? 'bg-primary-light text-white hover:bg-primary-dark' : 'text-gray-700 dark:text-gray-300'
              }`}
            >
              Chat
            </button>
            <button
              onClick={() => setCurrentView('settings')}
              className={`w-full text-left px-4 py-3 hover:bg-gray-100 dark:hover:bg-surface-dark transition-colors ${
                currentView === 'settings' ? 'bg-primary-light text-white hover:bg-primary-dark' : 'text-gray-700 dark:text-gray-300'
              }`}
            >
              Settings
            </button>
          </nav>
        </div>

        {/* Main content */}
        <div className="flex-1 overflow-hidden">
          <CurrentComponent />
        </div>
      </motion.div>

      {/* File Transfer Modal */}
      {fileTransfer && (
        <FileTransferModal
          file={fileTransfer}
          onClose={() => setFileTransfer(null)}
          onPause={() => setFileTransfer({ ...fileTransfer, paused: true })}
          onResume={() => setFileTransfer({ ...fileTransfer, paused: false })}
          onCancel={() => setFileTransfer(null)}
        />
      )}
    </div>
  );
}

export default App;