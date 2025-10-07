/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./src/**/*.{js,jsx,ts,tsx}",
  ],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        primary: {
          light: '#2563eb',
          dark: '#3b82f6',
        },
        neutral: '#64748b',
        background: {
          light: '#ffffff',
          dark: '#0f172a',
        },
        surface: {
          light: '#f9fafb',
          dark: '#1e293b',
        },
        error: '#dc2626',
        success: '#16a34a',
        warning: '#fbbf24',
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', '-apple-system', 'sans-serif'],
      },
      spacing: {
        'unit': '8px',
      },
      borderRadius: {
        'card': '12px',
        'modal': '20px',
      },
      animation: {
        'fast': '150ms ease-out',
        'medium': '300ms ease-out',
        'slow': '600ms ease-out',
      },
      screens: {
        'sm': '640px',
        'md': '768px',
        'lg': '1024px',
        'xl': '1280px',
        '2xl': '1536px',
      },
    },
  },
  plugins: [],
}