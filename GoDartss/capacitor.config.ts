import { CapacitorConfig } from '@capacitor/cli';

// URL du serveur Next.js local (machine avec les cameras).
// Override avec la variable d'environnement GODARTS_SERVER_URL au moment du build.
//   ex: GODARTS_SERVER_URL=http://192.168.1.42:3000 npm run build:android
const serverUrl = process.env.GODARTS_SERVER_URL || 'http://192.168.1.100:3000';

const config: CapacitorConfig = {
  appId: 'com.godarts.app',
  appName: 'GoDarts',
  webDir: 'www',
  server: {
    // Charge l'app directement depuis le serveur local — pas besoin de build statique.
    url: serverUrl,
    cleartext: true, // autorise HTTP (non-HTTPS) sur réseau local
  },
  android: {
    allowMixedContent: true, // nécessaire pour les ressources HTTP en mode local
    captureInput: true,
    webContentsDebuggingEnabled: true, // debug via Chrome DevTools (désactiver en prod)
  },
};

export default config;
