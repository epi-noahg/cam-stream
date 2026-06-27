/**
 * Script utilitaire: met à jour l'URL du serveur dans capacitor.config.ts
 * Usage: npx tsx scripts/set-server-ip.ts 192.168.1.42 3000
 */

import * as fs from 'fs';
import * as path from 'path';

const ip   = process.argv[2];
const port = process.argv[3] || '3000';

if (!ip) {
  console.error('Usage: npx tsx scripts/set-server-ip.ts <IP> [PORT]');
  console.error('Exemple: npx tsx scripts/set-server-ip.ts 192.168.1.42 3000');
  process.exit(1);
}

const configPath = path.join(__dirname, '..', 'capacitor.config.ts');
let content = fs.readFileSync(configPath, 'utf-8');

// Remplace la ligne d'URL par défaut
content = content.replace(
  /const serverUrl = process\.env\.GODARTS_SERVER_URL \|\| '[^']+';/,
  `const serverUrl = process.env.GODARTS_SERVER_URL || 'http://${ip}:${port}';`
);

fs.writeFileSync(configPath, content);
console.log(`✅ URL serveur mise à jour: http://${ip}:${port}`);
console.log('   Relancez "npm run android:sync" pour appliquer le changement.');
