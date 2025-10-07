import { createSlice } from '@reduxjs/toolkit';

const settingsSlice = createSlice({
  name: 'settings',
  initialState: {
    theme: 'light',
    notifications: true,
    autoAcceptFiles: false,
  },
  reducers: {
    setTheme: (state, action) => {
      state.theme = action.payload;
    },
    setNotifications: (state, action) => {
      state.notifications = action.payload;
    },
    setAutoAcceptFiles: (state, action) => {
      state.autoAcceptFiles = action.payload;
    },
  },
});

export const { setTheme, setNotifications, setAutoAcceptFiles } = settingsSlice.actions;
export default settingsSlice.reducer;