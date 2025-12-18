use crate::transport::Transport;
use crate::Result;
use std::sync::Arc;

#[async_trait::async_trait]
pub trait Connection: Send + Sync {
    async fn disconnect(&self) -> Result<()>;
    fn is_connected(&self) -> bool;
    async fn create_transport(&self, service_uuid: uuid::Uuid) -> Result<Arc<dyn Transport>>;
}