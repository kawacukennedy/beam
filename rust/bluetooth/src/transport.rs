use crate::Result;
use tokio::sync::mpsc;

#[async_trait::async_trait]
pub trait Transport: Send + Sync {
    async fn send_packet(&self, data: &[u8]) -> Result<()>;
    async fn receive_packets(&self) -> Result<mpsc::Receiver<Vec<u8>>>;
    async fn close(&self) -> Result<()>;
}