import Head from 'next/head';
import styles from '../styles/Home.module.css';
import Header from '../components/header';
import { TextField, Button, Avatar } from '@mui/material';
import { useEffect, useState } from 'react';


type User = {
  id: string
  username: string
}

type Message = {
  id: string
  user: User
  content: string
  timestamp: Date
  roomId: string
}

type Room = {
  id: string
  name: string
}

function genRandomName() {
  const number = Math.floor(Math.random() * 999999);
  return `user-${number}`
}

function getStoredUser(): User {
  const user = localStorage.getItem('servertech_user')
  if (user) {
    return JSON.parse(user)
  } else {
    return { id: crypto.randomUUID(), username: genRandomName() }
  }
}

const rooms: Room[] = [
  { id: 'room-1', name: 'Boost.Beast' },
  { id: 'room-2', name: 'Building Boost' },
  { id: 'room-3', name: 'Building Boost' },
  { id: 'room-4', name: 'Building Boost' },
  { id: 'room-5', name: 'Building Boost' },
  { id: 'room-6', name: 'Building Boost' },
  { id: 'room-7', name: 'Building Boost' },
  { id: 'room-8', name: 'Building Boost' },
  { id: 'room-9', name: 'Building Boost' },
  { id: 'room-10', name: 'Building Boost' },
  { id: 'room-11', name: 'Building Boost' },
  { id: 'room-12', name: 'Building Boost' },
  { id: 'room-13', name: 'Building Boost' },
  { id: 'room-14', name: 'Building Boost' },
  { id: 'room-15', name: 'Building Boost' },
]

// Copied from MUI Avatar docs
function stringToColor(string: string) {
  let hash = 0;
  let i;

  /* eslint-disable no-bitwise */
  for (i = 0; i < string.length; i += 1) {
    hash = string.charCodeAt(i) + ((hash << 5) - hash);
  }

  let color = '#';

  for (i = 0; i < 3; i += 1) {
    const value = (hash >> (i * 8)) & 0xff;
    color += `00${value.toString(16)}`.slice(-2);
  }
  /* eslint-enable no-bitwise */

  return color;
}

function formatDate(date: Date): string {
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: 'medium',
    timeStyle: 'short'
  }).format(date)
}

const NameAvatar = ({ name }: { name: string }) => {
  return <Avatar sx={{
    bgcolor: stringToColor(name),
  }}>{name[0]}</Avatar>
}

const RoomEntry = ({ name }: { name: string }) => {
  return (
    <div className='flex p-3'>
      <div className='pr-3 flex flex-col justify-center'>
        <NameAvatar name={name} />
      </div>
      <div className='flex-1 flex flex-col'>
        <p className='m-0 font-bold pb-2'>{name}</p>
        <p className='m-0'>Room last message</p>
      </div>
    </div>
  )
}

const OtherUserMessage = ({ username, content, timestamp }: { username: string, content: string, timestamp: Date }) => {
  return (
    <div className='flex flex-row pt-3 pb-3'>
      <div className='pr-5 flex flex-col justify-end'>
        <NameAvatar name={username} />
        <p className='m-0 pt-1 text-sm'>User 1</p>
      </div>
      <div className='flex-[6] bg-white rounded-lg pt-4 pl-5 pr-2'>
        <p className='m-0'>{content}</p>
        <p className='m-0 pb-2 pr-2 text-xs text-right' style={{ color: 'var(--boost-grey)' }}>{formatDate(timestamp)}</p>
      </div>
      <div className='flex-[3]'>
      </div>
    </div>
  )
}

const MyMessage = ({ content, timestamp }: { content: string, timestamp: Date }) => {
  return (
    <div className='flex flex-row pt-3 pb-3'>
      <div className='flex-[4]'>
      </div>
      <div className='flex-[6] bg-green-100 rounded-lg pt-4 pl-5 pr-2'>
        <p className='m-0'>{content}</p>
        <p className='m-0 pb-2 pr-2 text-xs text-right' style={{ color: 'var(--boost-grey)' }}>{formatDate(timestamp)}</p>
      </div>
    </div>
  )
}

const testMessages = [
  { username: 'User 1', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 2', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 3', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 4', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 5', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 6', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 7', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 8', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 9', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 10', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 11', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 12', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 13', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 14', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 15', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 16', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 17', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
  { username: 'User 18', content: 'Their messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir messageTheir message', timestamp: new Date(Date.UTC(2020, 11, 20, 9, 23, 16, 738)) },
]

export default function Home() {
  const [user, setUser] = useState(null);
  useEffect(() => {
    setUser(getStoredUser());
  }, [])
  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>
      <div className="flex flex-col h-full">
        <Header />
        <div className="flex-1 flex min-h-0" style={{ borderTop: '1px solid var(--boost-light-grey)' }}>
          <div className='flex-1 flex flex-col overflow-y-scroll'>
            {rooms.map(({ id, name }) => <RoomEntry key={id} name={name} />)}
          </div>
          <div className='flex-[3] flex overflow-y-scroll' style={{ backgroundColor: 'var(--boost-light-grey)' }}>
            <div className='flex-1 flex flex-col p-5'>
              {testMessages.map(({ username, content, timestamp }) =>
                <>
                  <OtherUserMessage
                    key={username}
                    username={username}
                    content={content}
                    timestamp={timestamp}
                  />
                  <MyMessage
                    content={content}
                    timestamp={timestamp}
                  />
                </>
              )
              }
            </div>
          </div>
        </div>
      </div>
    </>
  )
}
