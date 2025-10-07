import React, { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import { useDispatch, useSelector } from 'react-redux';
import { setDevices, setLoading } from '../slices/devicesSlice';

const DeviceDiscovery = () => {
  const dispatch = useDispatch();
  const { list: devices, loading } = useSelector((state) => state.devices);

  const discoverDevices = async () => {
    dispatch(setLoading(true));
    try {
      const { discovered, stored } = await window.electronAPI.getDevices();
      const allDevices = [...discovered, ...stored];
      dispatch(setDevices(allDevices));
    } catch (error) {
      console.error('Error discovering devices:', error);
    }
    dispatch(setLoading(false));
  };

  useEffect(() => {
    discoverDevices();
    const interval = setInterval(discoverDevices, 5000); // Auto-refresh every 5s
    return () => clearInterval(interval);
  }, [dispatch]);

  const connectToDevice = async (device) => {
    try {
      const deviceId = await window.electronAPI.addDevice(device);
      dispatch(setConnectedDevice({ ...device, id: deviceId }));
      // TODO: Navigate to chat view
    } catch (error) {
      console.error('Error connecting to device:', error);
    }
  };

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold mb-4">Device Discovery</h1>
      <button
        onClick={discoverDevices}
        disabled={loading}
        className="bg-primary-light dark:bg-primary-dark text-white px-4 py-2 rounded-lg mb-4"
      >
        {loading ? 'Scanning...' : 'Refresh Devices'}
      </button>
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {devices.map((device, index) => (
          <motion.div
            key={device.id || device.address}
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: index * 0.1 }}
            className="bg-white dark:bg-slate-800 p-4 rounded-lg shadow-md"
          >
            <h3 className="font-semibold">{device.name}</h3>
            <p className="text-sm text-gray-600 dark:text-gray-400">{device.bluetooth_address || device.address}</p>
            {device.signalStrength && (
              <div className="mt-2">
                <span className="text-xs bg-green-100 dark:bg-green-900 px-2 py-1 rounded">
                  Signal: {device.signalStrength}%
                </span>
              </div>
            )}
            <button
              onClick={() => connectToDevice(device)}
              className="mt-2 bg-secondary-light dark:bg-secondary-dark text-white px-3 py-1 rounded text-sm"
            >
              Connect
            </button>
          </motion.div>
        ))}
      </div>
    </div>
  );
};

export default DeviceDiscovery;