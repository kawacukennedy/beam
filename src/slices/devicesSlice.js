import { createSlice } from '@reduxjs/toolkit';

const devicesSlice = createSlice({
  name: 'devices',
  initialState: {
    list: [],
    connected: null,
    loading: false,
  },
  reducers: {
    setDevices: (state, action) => {
      state.list = action.payload;
    },
    setConnectedDevice: (state, action) => {
      state.connected = action.payload;
    },
    setLoading: (state, action) => {
      state.loading = action.payload;
    },
  },
});

export const { setDevices, setConnectedDevice, setLoading } = devicesSlice.actions;
export default devicesSlice.reducer;