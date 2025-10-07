import { createSlice } from '@reduxjs/toolkit';

const messagesSlice = createSlice({
  name: 'messages',
  initialState: {
    list: [],
  },
  reducers: {
    addMessage: (state, action) => {
      state.list.push(action.payload);
    },
    setMessages: (state, action) => {
      state.list = action.payload;
    },
  },
});

export const { addMessage, setMessages } = messagesSlice.actions;
export default messagesSlice.reducer;