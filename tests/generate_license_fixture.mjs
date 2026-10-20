import { generateKeyPairSync, sign } from 'node:crypto';
import { writeFileSync } from 'node:fs';
// Ephemeral test material, never used by a product build.
const { publicKey, privateKey } = generateKeyPairSync('ed25519');
const device = 'v2:' + 'a'.repeat(64);
const payload = Buffer.from(JSON.stringify({
  version: 2, product: 'NEUROKORE',
  licenseId: '00000000-0000-4000-8000-000000000000',
  device, tier: 'full', email: 'test@example.com', issuedAt: 1000, expiresAt: 2000,
})).toString('base64');
const license = JSON.stringify({
  format: 'NEUROKORE LICENSE', version: 2, payload,
  signature: sign(null, Buffer.from('NEUROKORE-LICENSE-V2\n' + payload), privateKey).toString('base64'),
});
writeFileSync(process.argv[2], JSON.stringify({
  license, device,
  publicKey: publicKey.export({ format: 'der', type: 'spki' }).subarray(-32).toString('hex'),
}));
