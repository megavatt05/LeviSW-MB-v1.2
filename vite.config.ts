import tailwindcss from '@tailwindcss/vite';
import react from '@vitejs/plugin-react';
import fs from 'fs';
import path from 'path';
import {defineConfig} from 'vite';

export default defineConfig(() => {
  return {
    plugins: [
      react(),
      tailwindcss(),
      // Копирует src/assets в dist/, чтобы корневые относительные URL-адреса, такие как
      // "/src/assets/images/..." (например, результат generate_image), продолжали открываться
      // в production-сборке. Не удалять.
      {
        name: 'copy-unbundled-src-assets',
        apply: 'build',
        closeBundle(error?: Error): void {
          if (error) {
            return;
          }
          const distDir = path.resolve(__dirname, 'dist');
          const srcAssets = path.resolve(__dirname, 'src/assets');
          const distAssets = path.resolve(distDir, 'src/assets');
          if (fs.existsSync(distDir) && fs.existsSync(srcAssets)) {
            fs.cpSync(srcAssets, distAssets, {recursive: true, force: true});
          }
        },
      },
    ],
    resolve: {
      alias: {
        '@': path.resolve(__dirname, '.'),
      },
    },
    server: {
      // HMR отключается в AI Studio с помощью переменной окружения DISABLE_HMR.
      // Не изменять: наблюдение за файлами отключается, чтобы избежать мерцания во время правок агентом.
      hmr: process.env.DISABLE_HMR !== 'true',
      // Отключать наблюдение за файлами при DISABLE_HMR=true, чтобы снизить нагрузку на процессор во время правок агентом.
      watch: process.env.DISABLE_HMR === 'true' ? null : {},
    },
  };
});
