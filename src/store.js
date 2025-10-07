import { configureStore } from '@reduxjs/toolkit';
import devicesReducer from './slices/devicesSlice';
import messagesReducer from './slices/messagesSlice';
import settingsReducer from './slices/settingsSlice';

export const store = configureStore({
  reducer: {
    devices: devicesReducer,
    messages: messagesReducer,
    settings: settingsReducer,
  },
});