import React, { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import { X, File, Pause, Play, Download } from 'lucide-react';

const FileTransferModal = ({ file, onClose, onPause, onResume, onCancel }) => {
  const [progress, setProgress] = useState(0);
  const [speed, setSpeed] = useState(0);
  const [eta, setEta] = useState(0);
  const [isPaused, setIsPaused] = useState(false);

  useEffect(() => {
    // Simulate progress
    const interval = setInterval(() => {
      setProgress(prev => {
        if (prev >= 100) {
          clearInterval(interval);
          return 100;
        }
        return prev + 5;
      });
      setSpeed(Math.random() * 2 + 1); // MB/s
      setEta(Math.max(0, (100 - progress) / (speed * 10))); // seconds
    }, 200);

    return () => clearInterval(interval);
  }, [progress, speed]);

  const formatSize = (bytes) => {
    const sizes = ['B', 'KB', 'MB', 'GB'];
    if (bytes === 0) return '0 B';
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return Math.round(bytes / Math.pow(1024, i) * 100) / 100 + ' ' + sizes[i];
  };

  const formatTime = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
      <motion.div
        initial={{ opacity: 0, scale: 0.9 }}
        animate={{ opacity: 1, scale: 1 }}
        exit={{ opacity: 0, scale: 0.9 }}
        className="bg-white dark:bg-surface-dark rounded-modal shadow-2xl max-w-md w-full mx-4"
      >
        <div className="p-6">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-xl font-semibold">File Transfer</h2>
            <button onClick={onClose} className="text-gray-400 hover:text-gray-600">
              <X className="w-6 h-6" />
            </button>
          </div>

          <div className="flex items-center space-x-4 mb-4">
            <File className="w-12 h-12 text-primary-light" />
            <div>
              <p className="font-medium">{file.filename}</p>
              <p className="text-sm text-gray-600 dark:text-gray-400">
                {formatSize(file.size)}
              </p>
            </div>
          </div>

          <div className="mb-4">
            <div className="flex justify-between text-sm mb-1">
              <span>{Math.round(progress)}%</span>
              <span>{formatSize(file.size * progress / 100)} / {formatSize(file.size)}</span>
            </div>
            <div className="w-full bg-gray-200 dark:bg-gray-700 rounded-full h-2">
              <motion.div
                className="bg-primary-light h-2 rounded-full"
                initial={{ width: 0 }}
                animate={{ width: `${progress}%` }}
                transition={{ duration: 0.2 }}
              />
            </div>
          </div>

          <div className="flex justify-between text-sm text-gray-600 dark:text-gray-400 mb-6">
            <span>Speed: {speed.toFixed(1)} MB/s</span>
            <span>ETA: {formatTime(eta)}</span>
          </div>

          <div className="flex space-x-2">
            <button
              onClick={isPaused ? onResume : onPause}
              className="flex-1 bg-gray-200 dark:bg-gray-700 hover:bg-gray-300 dark:hover:bg-gray-600 text-gray-800 dark:text-gray-200 py-2 px-4 rounded-lg flex items-center justify-center"
            >
              {isPaused ? <Play className="w-4 h-4 mr-2" /> : <Pause className="w-4 h-4 mr-2" />}
              {isPaused ? 'Resume' : 'Pause'}
            </button>
            <button
              onClick={onCancel}
              className="flex-1 bg-error hover:bg-red-600 text-white py-2 px-4 rounded-lg"
            >
              Cancel
            </button>
          </div>

          {progress === 100 && (
            <motion.div
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              className="mt-4 p-3 bg-success bg-opacity-10 border border-success rounded-lg flex items-center"
            >
              <Download className="w-5 h-5 text-success mr-2" />
              <span className="text-success font-medium">Transfer Complete</span>
            </motion.div>
          )}
        </div>
      </motion.div>
    </div>
  );
};

export default FileTransferModal;