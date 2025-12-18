use crate::Result;
use uuid::Uuid;

#[async_trait::async_trait]
pub trait Device: Send + Sync {
    fn id(&self) -> String;
    fn name(&self) -> Option<String>;
    fn address(&self) -> String;
    fn is_connected(&self) -> bool;
    fn services(&self) -> Vec<Uuid>;
}