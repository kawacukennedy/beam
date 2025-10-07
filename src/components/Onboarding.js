import React, { useState } from 'react';
import { motion } from 'framer-motion';
import { ChevronRight, Bluetooth, User, CheckCircle } from 'lucide-react';

const Onboarding = ({ onComplete }) => {
  const [step, setStep] = useState(0);
  const [profile, setProfile] = useState({ name: '', avatar: null });

  const steps = [
    {
      title: 'Welcome to BlueBeam',
      content: 'Offline. Private. Instant Bluetooth Communication.',
      icon: <Bluetooth className="w-16 h-16 text-primary-light" />,
    },
    {
      title: 'Enable Bluetooth',
      content: 'We need Bluetooth permission to discover and connect to nearby devices.',
      icon: <Bluetooth className="w-16 h-16 text-primary-light" />,
      action: 'Grant Permission',
    },
    {
      title: 'Set Up Your Profile',
      content: 'Choose a display name and optional avatar.',
      icon: <User className="w-16 h-16 text-primary-light" />,
      form: true,
    },
    {
      title: 'Tutorial',
      content: 'Learn how to use BlueBeam: Discover devices in the sidebar, chat in the main panel.',
      icon: <CheckCircle className="w-16 h-16 text-success" />,
    },
  ];

  const nextStep = () => {
    if (step < steps.length - 1) {
      setStep(step + 1);
    } else {
      onComplete();
    }
  };

  const currentStep = steps[step];

  return (
    <div className="min-h-screen bg-gradient-to-br from-primary-light to-primary-dark flex items-center justify-center p-6">
      <motion.div
        initial={{ opacity: 0, scale: 0.9 }}
        animate={{ opacity: 1, scale: 1 }}
        transition={{ duration: 0.3 }}
        className="bg-white dark:bg-surface-dark rounded-modal shadow-2xl max-w-md w-full p-8 text-center"
      >
        <motion.div
          key={step}
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.3 }}
        >
          <div className="mb-6">{currentStep.icon}</div>
          <h1 className="text-2xl font-bold text-gray-900 dark:text-gray-100 mb-4">
            {currentStep.title}
          </h1>
          <p className="text-gray-600 dark:text-gray-400 mb-6">
            {currentStep.content}
          </p>

          {currentStep.form && (
            <div className="space-y-4 mb-6">
              <input
                type="text"
                placeholder="Your display name"
                value={profile.name}
                onChange={(e) => setProfile({ ...profile, name: e.target.value })}
                className="w-full px-4 py-3 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-surface-dark focus:ring-2 focus:ring-primary-light focus:border-transparent"
              />
              <input
                type="file"
                accept="image/*"
                onChange={(e) => setProfile({ ...profile, avatar: e.target.files[0] })}
                className="w-full px-4 py-3 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-surface-dark"
              />
            </div>
          )}

          <button
            onClick={nextStep}
            disabled={currentStep.form && !profile.name.trim()}
            className="w-full bg-primary-light hover:bg-primary-dark text-white font-semibold py-3 px-6 rounded-lg transition-colors duration-fast flex items-center justify-center disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {currentStep.action || 'Continue'}
            <ChevronRight className="w-5 h-5 ml-2" />
          </button>
        </motion.div>

        <div className="mt-6 flex justify-center space-x-2">
          {steps.map((_, index) => (
            <div
              key={index}
              className={`w-2 h-2 rounded-full ${
                index === step ? 'bg-primary-light' : 'bg-gray-300 dark:bg-gray-600'
              }`}
            />
          ))}
        </div>
      </motion.div>
    </div>
  );
};

export default Onboarding;