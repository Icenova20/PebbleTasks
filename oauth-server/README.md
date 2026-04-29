# PebbleTasks OAuth server

Hosts the **settings** page (Stay local vs Use Google Tasks) and Google OAuth 2.0 for the Pebble companion app.

## Google Cloud setup

1. Open [Google Cloud Console](https://console.cloud.google.com/) and create or select a project.
2. Enable **Google Tasks API** (APIs & Services → Library → “Google Tasks API” → Enable).
3. Configure **OAuth consent screen** (External or Internal) with at least the `.../auth/tasks` scope.
4. **Credentials** → Create credentials → **OAuth client ID** → Application type **Web application**.
5. Add **Authorized redirect URIs**:
   - Production: `https://YOUR_DOMAIN/oauth/callback`
   - Local dev: `http://localhost:3847/oauth/callback` (match `PORT` and `BASE_PUBLIC_URL`).
6. Copy the **Client ID** and **Client secret**.

## Run locally

```bash
cd oauth-server
cp .env.example .env
# Edit .env with your Google credentials and BASE_PUBLIC_URL=http://localhost:3847
npm install
npm start
```

Use HTTPS in production; `BASE_PUBLIC_URL` must match the public URL Pebble’s WebView will load (no trailing slash).

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | Settings UI: mode (local / Google) + optional “Connect Google” |
| GET | `/oauth/start` | Starts Google OAuth (`return_to` query required for Pebble callback) |
| GET | `/oauth/callback` | Google redirects here; exchanges code; redirects to `return_to` with tokens in URL fragment |
| POST | `/oauth/refresh` | JSON body `{ "refresh_token": "..." }` → returns new `access_token` and `expires_in` (keeps client secret on server) |

## Pebble app configuration

In the watch project, set `src/pkjs/oauth_config.js` `settingsBaseUrl` to this server’s public URL (same origin as `BASE_PUBLIC_URL`).

## Security notes

- Never commit `.env` or log refresh tokens.
- Use TLS for production OAuth redirects allowed by Google.
