import React, { useState, useEffect, useRef } from 'react';
import { motion } from 'framer-motion';
import { Send, Paperclip, Search, Smile } from 'lucide-react';
import { useSelector } from 'react-redux';

const ChatWindow = () => {
  const [messages, setMessages] = useState([]);
  const [newMessage, setNewMessage] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const messagesEndRef = useRef(null);
  const connectedDevice = useSelector((state) => state.devices.connected);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  useEffect(() => {
    if (connectedDevice) {
      // Load messages for the connected device
      loadMessages(connectedDevice.id);
    }
  }, [connectedDevice]);

  const loadMessages = async (deviceId) => {
    try {
      const msgs = await window.electronAPI.getMessages(deviceId);
      setMessages(msgs);
    } catch (error) {
      console.error('Error loading messages:', error);
    }
  };

  const sendMessage = async () => {
    if (!newMessage.trim() || !connectedDevice) return;

    const message = {
      senderId: 'local', // TODO: Get actual user ID
      receiverId: connectedDevice.id,
      content: newMessage,
      timestamp: new Date().toISOString(),
    };

    try {
      await window.electronAPI.sendMessage(message);
      setMessages([...messages, { ...message, status: 'sent' }]);
      setNewMessage('');
    } catch (error) {
      console.error('Error sending message:', error);
    }
  };

  const handleKeyPress = (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  };

  const filteredMessages = messages.filter(msg =>
    msg.content.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="flex flex-col h-full">
      {/* Header */}
      <div className="bg-white dark:bg-surface-dark border-b border-gray-200 dark:border-gray-700 p-4">
        <div className="flex items-center justify-between">
          <h1 className="text-xl font-semibold">
            {connectedDevice ? `Chat with ${connectedDevice.name}` : 'Select a device to chat'}
          </h1>
          <div className="relative">
            <Search className="w-5 h-5 absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400" />
            <input
              type="text"
              placeholder="Search messages..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="pl-10 pr-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-surface-dark focus:ring-2 focus:ring-primary-light focus:border-transparent"
            />
          </div>
        </div>
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto p-4 space-y-4">
        {filteredMessages.map((message, index) => (
          <motion.div
            key={message.id || index}
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ duration: 0.2 }}
            className={`flex ${message.senderId === 'local' ? 'justify-end' : 'justify-start'}`}
          >
            <div
              className={`max-w-xs lg:max-w-md px-4 py-2 rounded-lg ${
                message.senderId === 'local'
                  ? 'bg-primary-light text-white'
                  : 'bg-surface-light dark:bg-surface-dark text-gray-900 dark:text-gray-100'
              }`}
            >
              <p className="text-body">{message.content}</p>
              <p className="text-caption text-gray-500 dark:text-gray-400 mt-1">
                {new Date(message.timestamp).toLocaleTimeString()}
                {message.status && ` • ${message.status}`}
              </p>
            </div>
          </motion.div>
        ))}
        <div ref={messagesEndRef} />
      </div>

      {/* Input */}
      {connectedDevice && (
        <div className="bg-white dark:bg-surface-dark border-t border-gray-200 dark:border-gray-700 p-4">
          <div className="flex items-center space-x-2">
            <button className="p-2 text-gray-400 hover:text-gray-600 dark:hover:text-gray-200">
              <Paperclip className="w-5 h-5" />
            </button>
            <textarea
              value={newMessage}
              onChange={(e) => setNewMessage(e.target.value)}
              onKeyPress={handleKeyPress}
              placeholder="Type a message..."
              rows={1}
              className="flex-1 px-4 py-2 border border-gray-300 dark:border-gray-600 rounded-lg bg-white dark:bg-surface-dark focus:ring-2 focus:ring-primary-light focus:border-transparent resize-none"
            />
            <button className="p-2 text-gray-400 hover:text-gray-600 dark:hover:text-gray-200">
              <Smile className="w-5 h-5" />
            </button>
            <button
              onClick={sendMessage}
              disabled={!newMessage.trim()}
              className="p-2 bg-primary-light hover:bg-primary-dark text-white rounded-lg disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Send className="w-5 h-5" />
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default ChatWindow;