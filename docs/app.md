
NexUrbis

Here is the corrected and expanded Architecture & Design Document. 

### What was fixed regarding NGSI-LD and Federation?
In your original draft, **Federation (Section 3.3)** was described using NGSI-LD *Subscriptions*. While syncing data via subscriptions (Context Replication) is common, **true NGSI-LD Federation relies on Context Source Registrations (CSR)** according to the ETSI NGSI-LD specification. I have corrected this section so it relies on the `/ngsi-ld/v1/csourceRegistrations` endpoint. This allows a central broker to transparently route queries to edge brokers without duplicating the data.

I have also fully expanded **Section 4** to include the exact table names (using the `tbl_` prefix), columns, data types, and Enums required for the PostgreSQL TypeORM schema.

---

# Smart City Management Portal: Architecture & Design Document

## 1. Introduction
This document outlines the architecture and technical design of the Smart City Management Portal. This web application acts as the unified control plane for the FIWARE-based Digital Twin infrastructure. It simplifies the orchestration of IoT devices, NGSI-LD entities, subscriptions, ETSI-compliant context federation, and fine-grained access control, abstracting the complexity of the underlying FIWARE components.

## 2. Technology Stack
*   **Frontend:** ReactJS (Context API, React Query for state management, TailwindCSS for UI).
*   **Backend:** NestJS (Modular Node.js framework).
*   **Database:** PostgreSQL (Internal app state managed via TypeORM).
*   **API Layer:** oRPC (End-to-end typesafe RPC framework connecting React and NestJS).
*   **Target Infrastructure Managed:** Orion-LD (Context Broker), FIWARE IoT Agents (JSON, LoRaWAN), Keycloak (IAM/OIDC), Kong (API Gateway).

## 3. Core Modules & Workflows

### 3.1. Device & Entity Management (IoT & Smart Data Models)
*   **Purpose:** Allow users to seamlessly onboard new sensors and map them to official Smart Data Models.
*   **Workflow:** User selects the input protocol and the Smart Data Model entity type. The backend dynamically generates a configuration form based on the model's JSON Schema. The user maps hardware payload keys to NGSI-LD properties. For MQTT devices, the user can optionally provide per-device MQTT credentials (username/password) if the broker requires them. NestJS provisions the device via the FIWARE IoT Agent API.

### 3.2. Subscription & Notification Engine
*   **Purpose:** Create NGSI-LD subscriptions to push context data changes to external systems.
*   **Workflow:** User defines trigger conditions (Entity Type, watched attributes) and an action (Webhook URL). NestJS posts this to Orion-LD (`/ngsi-ld/v1/subscriptions`).

### 3.3. Subscriptions, Federation & Synchronization
*   **Purpose:** Enable distributed Context Brokers to share data, save historical state, or physically copy data across domains.

**3.3.1. Classical Subscription (Database Sync / History)**
*   **Concept:** Used when you want to save historical state changes to a time-series database.
*   **Workflow:** QuantumLeap creates a subscription in Orion-LD. When data enters Orion-LD, it notifies QuantumLeap, which natively understands the NGSI-LD `"Notification"` wrapper, extracts the data, and stores it into CrateDB or TimescaleDB.

**3.3.2. Context Federation (Real-Time External Fetch)**
*   **Concept:** Relies on ETSI NGSI-LD **Context Source Registrations (CSR)**. A central Context Broker acts as a federator, routing queries dynamically to external brokers without moving all the data centrally.
*   **Workflow:** User registers the Edge Broker, NestJS creates a `ContextSourceRegistration`, and Orion-LD routes external queries/updates to the Edge Broker transparently. Write operations (PATCH/POST) can also be forwarded to the Master if the CSR allows it.

**3.3.3. One-Way Sync (Physical Data Replication)**
*   **Concept:** Physically copy data from one broker to another automatically using **Subscriptions**. Ideal for pushing selected data to a secondary broker (e.g., Municipality to University).
*   **Workflow:** The "Master" broker holds the mutable entity. A Subscription is created on the Master broker targeting an "Unwrapper" middleware. This middleware is strictly required because Orion-LD sends a `"Notification"` wrapper, but the receiving broker's `/ngsi-ld/v1/entityOperations/upsert` endpoint requires a JSON Array. The middleware extracts the `"data"` array and forwards it. To ensure data is only modified on the Master, Kong API Gateway on the Replica is configured to deny manual `POST/PATCH/DELETE` for replicated entitie
### 3.4. Identity, Security & Access Management (OIDC & UMA)
*   **Purpose:** Treat every NGSI-LD Entity as a protected resource using Keycloak User-Managed Access (UMA) to define strictly what a user owns and what they can modify.
*   **Entity Ownership:** When a user creates an entity, their ID is stored as `ownerId` in `tbl_entity`. The owner automatically receives full Keycloak scopes (`read`, `write`, `delete`).
*   **Access Grants:** 3rd-party apps or other users are given OIDC Client IDs. The owner can grant specific scopes (e.g., `read` only, preventing modification).
*   **Enforcement Workflow:** When a request hits the system (e.g., `PATCH /ngsi-ld/v1/entities/urn:ngsi-ld:Device:001`), Kong API Gateway intercepts it, validates the OIDC token against Keycloak, and checks if the token possesses the `write` scope for that specific entity's UMA resource. If not, Kong blocks the request with a 403 Forbidden before it ever reaches Orion-LD.

### 3.5. Application Data Management (Grouping)
*   **Purpose:** Logically group digital twin assets for easier management, batch operations, and bulk-assignment.
*   **Workflow:** The UI provides a dynamic form to create new groups (stored in `tbl_entity_group`). Users can construct custom queries and apply filters based on entity `type` (e.g., `StreetLight`) or `location` (e.g., drawing a bounding box on a map) to selectively bulk-assign entities into the group (tracked in `mtm_entity_entity_group`).

---

## 4. Internal Database Schema (TypeORM PostgreSQL)
*Note: While Orion-LD stores the actual real-time context data (temperature, location, etc.), the PostgreSQL database stores the relational management metadata required by the portal.*

### 4.1. Enums
```typescript
enum Enum_UserRole {
  SYS_ADMIN = 'SYS_ADMIN',
  CITY_MANAGER = 'CITY_MANAGER',
  DEVICE_OWNER = 'DEVICE_OWNER',
  DEVELOPER = 'DEVELOPER'
}

enum Enum_Protocol {
  MQTT_JSON = 'MQTT_JSON',
  LORAWAN = 'LORAWAN',
  HTTP = 'HTTP'
}

enum Enum_EntityVisibility {
  PUBLIC = 'PUBLIC',         // Open data, readable by anyone
  PRIVATE = 'PRIVATE',       // Only owner and explicitly granted apps can read
  FEDERATED = 'FEDERATED'    // Shared with specific federated brokers
}
```

### 4.2. Tables

**1. `tbl_user`**
Stores portal users (synced/linked with Keycloak).
*   `id` (UUID, Primary Key)
*   `keycloakSub` (VARCHAR, Unique) - Links to Keycloak user ID.
*   `email` (VARCHAR, Unique)
*   `fullName` (VARCHAR)
*   `role` (Enum_UserRole)
*   `createdAt` (TIMESTAMP)

**2. `tbl_entity`**
Stores portal metadata for NGSI-LD entities. (The actual real-time state lives in Orion-LD).
*   `id` (UUID, Primary Key)
*   `ngsiLdUrn` (VARCHAR, Unique) - e.g., `urn:ngsi-ld:AirQualityObserved:001`
*   `entityType` (VARCHAR) - e.g., `AirQualityObserved`
*   `ownerId` (UUID, Foreign Key -> `tbl_user.id`)
*   `smartDataModelUrl` (VARCHAR, Nullable) - Link to schema used.
*   `keycloakResourceId` (VARCHAR) - ID of the UMA resource generated in Keycloak.
*   `visibility` (Enum_EntityVisibility)
*   `createdAt` (TIMESTAMP)

**3. `tbl_device_config`**
Stores hardware to NGSI-LD mapping parameters for the IoT Agents.
*   `id` (UUID, Primary Key)
*   `entityId` (UUID, Foreign Key -> `tbl_entity.id`)
*   `hardwareDeviceId` (VARCHAR, Unique) - e.g., MAC address or LoRa DevEUI.
*   `protocol` (Enum_Protocol)
*   `apiKey` (VARCHAR) - FIWARE Service Path / API key used.
*   `payloadMapping` (JSONB) - Defines how raw `{"tmp": 22}` maps to NGSI-LD `temperature`.
*   `mqttUsername` (VARCHAR, Nullable) - Per-device MQTT username.
*   `mqttPasswordEncrypted` (VARCHAR, Nullable) - Per-device MQTT password (AES-256 encrypted).
*   `active` (BOOLEAN)

**4. `tbl_federation_target`**
Stores credentials and endpoints for other Context Brokers in the federated network.
*   `id` (UUID, Primary Key)
*   `organizationName` (VARCHAR)
*   `brokerUrl` (VARCHAR) - e.g., `http://edge-broker.city.local:1026`
*   `tenantHeader` (VARCHAR, Nullable) - NGSI-Tenant header if needed.
*   `authTokenEncrypted` (VARCHAR, Nullable) - Token to access the remote broker.

**5. `tbl_csource_registration`**
Tracks Context Source Registrations (federation query routing) for real-time external fetches.
*   `id` (UUID, Primary Key)
*   `ngsiRegistrationId` (VARCHAR) - ID returned by Orion-LD upon creation.
*   `federationTargetId` (UUID, Nullable, Foreign Key -> `tbl_federation_target.id`)
*   `entityTypeFilter` (VARCHAR) - e.g., `Vehicle`
*   `endpointUrl` (VARCHAR)
*   `ownerId` (UUID, Foreign Key -> `tbl_user.id`)

**6. `tbl_subscription`**
A relational table acting as the application's management layer for NGSI-LD subscriptions (Historical DB Sync, One-Way Sync).
*   `id` (UUID, Primary Key)
*   `ngsiSubscriptionId` (VARCHAR) - ID returned by Orion-LD upon creation.
*   `description` (VARCHAR)
*   `entityTypeFilter` (VARCHAR) - e.g., `WasteContainer`
*   `notificationEndpoint` (VARCHAR) - The webhook URL, QuantumLeap, or Unwrapper middleware URL.
*   `ownerId` (UUID, Foreign Key -> `tbl_user.id`)
*   `active` (BOOLEAN)
*   `createdAt` (TIMESTAMP)

**7. `tbl_external_app`**
Stores 3rd party applications built by developers that request access to city data.
*   `id` (UUID, Primary Key)
*   `developerId` (UUID, Foreign Key -> `tbl_user.id`)
*   `appName` (VARCHAR)
*   `keycloakClientId` (VARCHAR, Unique) - OIDC client ID generated for this app.
*   `description` (TEXT)
*   `webhookUrl` (VARCHAR, Nullable)

**8. `tbl_app_access_grant`**
Tracks UMA policies. When an Entity Owner grants a 3rd party App access to their sensor.
*   `id` (UUID, Primary Key)
*   `appId` (UUID, Foreign Key -> `tbl_external_app.id`)
*   `entityId` (UUID, Foreign Key -> `tbl_entity.id`)
*   `keycloakPolicyId` (VARCHAR) - Reference to the UMA policy in Keycloak.
*   `grantedScopes` (JSONB) - e.g., `["read", "write"]`
*   `grantedAt` (TIMESTAMP)

**9. `tbl_entity_group`**
Stores group definitions and metadata for logical grouping of entities (e.g., "Downtown Streetlights").
*   `id` (UUID, Primary Key)
*   `name` (VARCHAR)
*   `description` (TEXT)
*   `ownerId` (UUID, Foreign Key -> `tbl_user.id`)
*   `createdAt` (TIMESTAMP)

**10. `mtm_entity_entity_group`**
A many-to-many mapping table linking specific NGSI-LD entities to their respective groups.
*   `entityId` (UUID, Foreign Key -> `tbl_entity.id`)
*   `groupId` (UUID, Foreign Key -> `tbl_entity_group.id`)
*   `assignedAt` (TIMESTAMP)

---

## 5. System Orchestration Logic (NestJS Services)
*   **IotAgentService:** Manages `tbl_device_config` and orchestrates calls to FIWARE IoT Agents.
*   **OrionLdService:** Wrapper for all Orion-LD communications. Includes support for Batch Upsert operations (`/ngsi-ld/v1/entityOperations/upsert`) to bulk create or overwrite entities efficiently.
*   **FederationService:** Handles the translation of `tbl_federation_target` configurations into `ContextSourceRegistration` payloads applied via the `OrionLdService`.
*   **KeycloakAdminService:** Reacts to entity creation/deletion. Uses `@keycloak/keycloak-admin-client` to dynamically provision users, clients, resources, and map UMA policies tracked in `tbl_app_access_grant`.
*   **KongAdminService:** Interacts with Kong's Admin API to dynamically apply OIDC/UMA plugins to specific `/ngsi-ld/v1/entities/{id}` API routes.

## 6. Development Phasing
*   **Phase 1 - Scaffold:** Setup Nx workspace, NestJS, React, TypeORM schemas (`tbl_*`), and oRPC.
*   **Phase 2 - Core IoT:** Implement Device Manager (Input -> `tbl_device_config` -> IoT Agent -> Orion-LD). Connect Smart Data Models API to UI.
*   **Phase 3 - True Federation & Sync:** Implement the Federation and Subscriptions UI. Map `tbl_federation_target` entries to NGSI-LD Context Source Registrations (`/csourceRegistrations`), and manage `tbl_subscription` for One-Way Syncs (via Unwrapper) and Historical syncs (via QuantumLeap).
*   **Phase 4 - Security (Hardest):** Implement Keycloak lifecycle. Map `tbl_entity` creation to Keycloak Resource creation. Build UI for `tbl_app_access_grant` (giving 3rd party apps OIDC access to specific sensors).