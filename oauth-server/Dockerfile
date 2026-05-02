# Node 20 — multi-arch (amd64, arm64, arm/v7) for Raspberry Pi
FROM node:20-alpine

WORKDIR /app

# Install production dependencies only
COPY package.json package-lock.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY templates ./templates/
COPY static ./static/
COPY server.js ./

# Non-root user provided by the official image
USER node

EXPOSE 3847

ENV NODE_ENV=production
ENV PORT=3847

CMD ["node", "server.js"]
